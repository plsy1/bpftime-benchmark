#include <bpf_map/bpftime_hash_map.hpp>
#include <bpf_map/userspace/fix_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cerrno>
#include <linux/bpf.h>
#include <optional>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

using namespace boost::interprocess;
using namespace bpftime;

namespace
{
struct shm_cleanup {
	explicit shm_cleanup(const char *name) : name(name)
	{
		shared_memory_object::remove(name);
	}
	~shm_cleanup()
	{
		shared_memory_object::remove(name);
	}
	const char *name;
};
} // namespace

TEST_CASE("Fast modulo preserves the remainder",
	  "[maps][hash][fast-modulo]")
{
	const std::array<size_t, 8> divisors = {
		2, 3, 11, 257, 1031, 65537, 1000003,
		std::numeric_limits<uint32_t>::max()
	};
	const std::array<size_t, 8> edge_values = {
		0,
		1,
		2,
		std::numeric_limits<uint32_t>::max(),
		static_cast<size_t>(std::numeric_limits<uint32_t>::max()) + 1,
		std::numeric_limits<size_t>::max() / 2,
		std::numeric_limits<size_t>::max() - 1,
		std::numeric_limits<size_t>::max(),
	};

	for (size_t divisor : divisors) {
		const size_t reciprocal =
			bpftime_hasher::modulo_reciprocal(divisor);
		for (size_t value : edge_values) {
			REQUIRE(bpftime_hasher::fast_modulo(
					value, divisor, reciprocal) ==
				value % divisor);
		}

		size_t value = 0x9e3779b97f4a7c15ULL;
		for (size_t i = 0; i < 10000; ++i) {
			value = value * 6364136223846793005ULL + 1;
			REQUIRE(bpftime_hasher::fast_modulo(
					value, divisor, reciprocal) ==
				value % divisor);
		}
	}
}

TEST_CASE("Four-byte fixed hash preserves the upstream byte hash",
	  "[maps][hash][hash-fast4]")
{
	const std::array<std::array<uint8_t, 4>, 5> cases = { {
		{ 0x00, 0x00, 0x00, 0x00 },
		{ 0x01, 0x00, 0x00, 0x00 },
		{ 0xff, 0xff, 0xff, 0xff },
		{ 0x12, 0x34, 0x56, 0x78 },
		{ 0x78, 0x56, 0x34, 0x12 },
	} };

	for (const auto &bytes : cases) {
		size_t expected = 0;
		for (uint8_t byte : bytes)
			expected = expected * 31 + byte;
		REQUIRE(bpftime_hasher::hash_func(bytes.data(), bytes.size()) ==
			expected);
	}
}

TEST_CASE("Fixed hash preserves uint32 keys across delete and reinsert cycles",
	  "[maps][hash][hash-key-patterns]")
{
	static const char *shm_name = "bpftime_fix_hash_key_patterns_test";
	shm_cleanup cleanup(shm_name);
	managed_shared_memory segment(create_only, shm_name, 4 * 1024 * 1024);

	constexpr size_t capacity = 256;
	constexpr size_t entry_count = 230;
	constexpr size_t cycles = 3;

	const std::array<std::vector<uint32_t>, 4> patterns = { [&] {
		std::vector<uint32_t> keys;
		for (uint32_t i = 0; i < entry_count; ++i)
			keys.push_back(i);
		return keys;
	}(), [&] {
		std::vector<uint32_t> keys;
		for (uint32_t i = 0; i < entry_count; ++i)
			keys.push_back(i * 4096U + 7U);
		return keys;
	}(), [&] {
		std::vector<uint32_t> keys;
		for (uint32_t i = 0; i < entry_count; ++i)
			keys.push_back((i << 16U) | 0x55aaU);
		return keys;
	}(), [&] {
		std::vector<uint32_t> keys;
		for (uint32_t i = 0; i < entry_count; ++i)
			keys.push_back(i * 1664525U + 1013904223U);
		return keys;
	}() };

	for (const auto &keys : patterns) {
		auto sorted_keys = keys;
		std::sort(sorted_keys.begin(), sorted_keys.end());
		REQUIRE(std::adjacent_find(sorted_keys.begin(), sorted_keys.end()) ==
			sorted_keys.end());

		bpftime_hash_map map(segment, capacity, sizeof(uint32_t),
				     sizeof(uint64_t));
		for (uint64_t cycle = 0; cycle < cycles; ++cycle) {
			for (uint32_t key : keys) {
				const uint64_t value =
					(static_cast<uint64_t>(key) << 32U) ^ cycle;
				REQUIRE(map.elem_update(&key, &value));
			}
			for (uint32_t key : keys) {
				const auto *value = static_cast<const uint64_t *>(
					map.elem_lookup(&key));
				REQUIRE(value != nullptr);
				REQUIRE(*value ==
					((static_cast<uint64_t>(key) << 32U) ^ cycle));
			}
			for (uint32_t key : keys)
				REQUIRE(map.elem_delete(&key));
			for (uint32_t key : keys)
				REQUIRE(map.elem_lookup(&key) == nullptr);
		}
	}
}

TEST_CASE("Fixed hash map preserves non-four-byte key comparisons",
	  "[maps][hash][hash-key-size-fallback]")
{
	static const char *shm_name = "bpftime_fix_hash_key_size_fallback_test";
	shm_cleanup cleanup(shm_name);
	managed_shared_memory segment(create_only, shm_name, 65536);

	bpftime_hash_map map(segment, 8, 3, sizeof(uint64_t));
	const std::array<uint8_t, 3> first_key = { 0x10, 0x20, 0x30 };
	const std::array<uint8_t, 3> second_key = { 0x10, 0x20, 0x31 };
	const uint64_t first_value = 0x1111222233334444ULL;
	const uint64_t second_value = 0x5555666677778888ULL;

	REQUIRE(map.elem_update(first_key.data(), &first_value));
	REQUIRE(map.elem_update(second_key.data(), &second_value));
	REQUIRE(*static_cast<const uint64_t *>(
			map.elem_lookup(first_key.data())) == first_value);
	REQUIRE(*static_cast<const uint64_t *>(
			map.elem_lookup(second_key.data())) == second_value);
	REQUIRE(map.elem_delete(first_key.data()));
	REQUIRE(map.elem_lookup(first_key.data()) == nullptr);
	REQUIRE(*static_cast<const uint64_t *>(
			map.elem_lookup(second_key.data())) == second_value);
}

TEST_CASE("Fixed hash iteration scans every bucket",
	  "[maps][hash][iteration]")
{
	static const char *shm_name = "bpftime_fix_hash_iteration_test";
	shm_cleanup cleanup(shm_name);
	managed_shared_memory segment(create_only, shm_name, 65536);

	constexpr size_t max_entries = 10;
	fix_size_hash_map_impl map(segment, max_entries, sizeof(uint32_t),
				   sizeof(uint64_t));
	// The map's own table, to find a key of the last bucket (a table of
	// the same size has another seed).
	bpftime_hash_map &layout = map.table_for_test();
	REQUIRE(layout.bucket_count() > max_entries);

	uint32_t key = 0;
	for (;; ++key) {
		if (layout.bucket_of_key(&key) == layout.bucket_count() - 1)
			break;
	}
	uint64_t value = 0x123456789abcdef0ULL;
	REQUIRE(map.elem_update(&key, &value, 0) == 0);

	uint32_t next_key = 0;
	REQUIRE(map.map_get_next_key(nullptr, &next_key) == 0);
	REQUIRE(next_key == key);
}

TEST_CASE("Fixed hash lookup keeps a full-capacity chain and misses past it",
	  "[maps][hash][hash-probe-loop]")
{
	static const char *shm_name = "bpftime_fix_hash_probe_loop_test";
	shm_cleanup cleanup(shm_name);
	managed_shared_memory segment(create_only, shm_name, 65536);
	constexpr size_t capacity = 7; // Every element in one bucket's chain.

	for (size_t key_size : { 1U, 3U, 4U, 8U, 16U }) {
		for (size_t value_size : { 1U, 8U }) {
			CAPTURE(key_size, value_size);
			bpftime_hash_map map(segment, capacity, key_size, value_size);
			// All in one bucket's chain, distinct keys (one-byte keys
			// have only 256; the bucket depends on the map's seed). Offset
			// each key by one byte to also exercise unaligned input.
			std::vector<std::vector<std::vector<uint8_t>>> by_bucket(
				map.bucket_count());
			std::vector<std::vector<uint8_t>> keys;
			const uint32_t distinct = key_size == 1 ? 256 : 65536;
			for (uint32_t n = 0; keys.empty() && n < distinct; ++n) {
				std::vector<uint8_t> key(key_size + 1, 0);
				// Long keys share their first four bytes: equality must
				// inspect the suffix rather than just the uint32 prefix.
				const size_t offset = key_size > 4 ? key_size - 3 : 1;
				for (size_t byte = 0; byte < std::min(key_size, size_t{4});
				     ++byte)
					key[byte + offset] = static_cast<uint8_t>(n >> (8 * byte));
				auto &in = by_bucket[map.bucket_of_key(key.data() + 1)];
				in.push_back(std::move(key));
				if (in.size() == capacity + 1)
					keys = in;
			}
			REQUIRE(keys.size() == capacity + 1);

			auto verify_value = [&](size_t index) {
				const auto *value = static_cast<const uint8_t *>(
					map.elem_lookup(keys[index].data() + 1));
				REQUIRE(value != nullptr);
				for (size_t byte = 0; byte < value_size; ++byte)
					REQUIRE(value[byte] == index + 1);
			};
			for (size_t index = 0; index < capacity; ++index) {
				const std::vector<uint8_t> value(value_size, index + 1);
				REQUIRE(map.elem_update(keys[index].data() + 1, value.data()));
			}
			for (size_t index = 0; index < capacity; ++index)
				verify_value(index);
			REQUIRE(map.elem_lookup(keys.back().data() + 1) == nullptr);

			REQUIRE(map.elem_delete(keys[0].data() + 1));
			REQUIRE(map.elem_delete(keys[3].data() + 1));
			for (size_t index = 0; index < capacity; ++index) {
				if (index == 0 || index == 3)
					REQUIRE(map.elem_lookup(keys[index].data() + 1) == nullptr);
				else
					verify_value(index);
			}
			REQUIRE(map.elem_lookup(keys.back().data() + 1) == nullptr);
			for (size_t index = 0; index < capacity; ++index) {
				if (index != 0 && index != 3)
					REQUIRE(map.elem_delete(keys[index].data() + 1));
			}
			REQUIRE(map.elem_lookup(keys.back().data() + 1) == nullptr);
			REQUIRE(map.get_elem_count() == 0);
			for (size_t index = 0; index < capacity; ++index) {
				const std::vector<uint8_t> value(value_size, index + 1);
				REQUIRE(map.elem_update(keys[index].data() + 1, value.data()));
			}
			for (size_t index = 0; index < capacity; ++index)
				verify_value(index);
		}
	}
}

TEST_CASE("Fixed hash map honours update flags like the kernel",
	  "[maps][hash][flags]")
{
	static const char *shm_name = "bpftime_fix_hash_flags_test";
	shm_cleanup cleanup(shm_name);
	managed_shared_memory segment(create_only, shm_name, 65536);
	fix_size_hash_map_impl map(segment, 4, sizeof(uint32_t),
				   sizeof(uint64_t));
	auto value_of = [&](uint32_t key) {
		auto ptr = static_cast<const uint64_t *>(map.elem_lookup(&key));
		return ptr ? std::optional<uint64_t>(*ptr) : std::nullopt;
	};

	uint32_t key = 1;
	uint64_t value = 10;
	REQUIRE(map.elem_update(&key, &value, BPF_NOEXIST) == 0);
	value = 11;
	errno = 0;
	REQUIRE(map.elem_update(&key, &value, BPF_NOEXIST) == -1);
	REQUIRE(errno == EEXIST);
	REQUIRE(value_of(1) == 10);
	REQUIRE(map.elem_update(&key, &value, BPF_EXIST) == 0);
	REQUIRE(value_of(1) == 11);

	uint32_t missing = 2;
	errno = 0;
	REQUIRE(map.elem_update(&missing, &value, BPF_EXIST) == -1);
	REQUIRE(errno == ENOENT);
	REQUIRE_FALSE(value_of(2).has_value());

	// Lookup-then-update counter racing with a reader that deletes the
	// key (the malloc example): the stale count must not come back.
	uint64_t stale = *value_of(1) + 1;
	REQUIRE(map.elem_delete(&key) == 0);
	errno = 0;
	REQUIRE(map.elem_update(&key, &stale, BPF_EXIST) == -1);
	REQUIRE(errno == ENOENT);
	REQUIRE_FALSE(value_of(1).has_value());

	errno = 0;
	REQUIRE(map.elem_delete(&key) == -1);
	REQUIRE(errno == ENOENT);

	errno = 0;
	REQUIRE(map.elem_update(&key, &value, BPF_EXIST + 1) == -1);
	REQUIRE(errno == EINVAL);
	REQUIRE(map.elem_update(&key, &value, BPF_ANY | BPF_F_LOCK) == 0);

	// Full map: a new key is rejected, an existing one still updates.
	for (uint32_t k = 10; k < 13; k++)
		REQUIRE(map.elem_update(&k, &value, BPF_ANY) == 0);
	uint32_t extra = 20;
	errno = 0;
	REQUIRE(map.elem_update(&extra, &value, BPF_ANY) == -1);
	REQUIRE(errno == E2BIG);
	REQUIRE(map.elem_update(&key, &value, BPF_ANY) == 0);
}

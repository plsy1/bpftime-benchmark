#include <bpf_map/bpftime_hash_map.hpp>
#include <bpf_map/userspace/fix_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
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

TEST_CASE("Fixed hash iteration scans adjusted bucket count",
	  "[maps][hash][iteration]")
{
	static const char *shm_name = "bpftime_fix_hash_iteration_test";
	shm_cleanup cleanup(shm_name);
	managed_shared_memory segment(create_only, shm_name, 65536);

	constexpr size_t requested_buckets = 10;
	const size_t actual_buckets =
		bpftime_hasher::next_prime(requested_buckets);
	REQUIRE(actual_buckets > requested_buckets);

	fix_size_hash_map_impl map(segment, requested_buckets, sizeof(uint32_t),
				   sizeof(uint64_t));

	uint32_t key = 0;
	for (;; ++key) {
		if (bpftime_hasher::hash_func(&key, sizeof(key)) % actual_buckets ==
		    actual_buckets - 1)
			break;
	}
	uint64_t value = 0x123456789abcdef0ULL;
	REQUIRE(map.elem_update(&key, &value, 0) == 0);

	uint32_t next_key = 0;
	REQUIRE(map.map_get_next_key(nullptr, &next_key) == 0);
	REQUIRE(next_key == key);
}

TEST_CASE("Fixed hash lookup preserves wrapped probe chains and full-table misses",
	  "[maps][hash][hash-probe-loop]")
{
	static const char *shm_name = "bpftime_fix_hash_probe_loop_test";
	shm_cleanup cleanup(shm_name);
	managed_shared_memory segment(create_only, shm_name, 65536);
	constexpr size_t capacity = 7; // Prime: the table can have no empty buckets.

	for (size_t key_size : { 1U, 3U, 4U, 8U, 16U }) {
		for (size_t value_size : { 1U, 8U }) {
			CAPTURE(key_size, value_size);
			bpftime_hash_map map(segment, capacity, key_size, value_size);
			std::vector<std::vector<uint8_t>> keys;
			// Start at the final bucket so every later collision wraps.
			// Offset each key by one byte to also exercise unaligned input.
			for (uint32_t n = 0; keys.size() < capacity + 1 && n < 65536;
			     ++n) {
				std::vector<uint8_t> key(key_size + 1, 0);
				// Long keys share their first four bytes: equality must
				// inspect the suffix rather than just the uint32 prefix.
				const size_t offset = key_size > 4 ? key_size - 3 : 1;
				for (size_t byte = 0; byte < std::min(key_size, size_t{4});
				     ++byte)
					key[byte + offset] = static_cast<uint8_t>(n >> (8 * byte));
				if (bpftime_hasher::hash_func(key.data() + 1, key_size) %
					    capacity == capacity - 1)
					keys.push_back(std::move(key));
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
			// No empty buckets remain: every bucket is now a tombstone.
			REQUIRE(map.elem_lookup(keys.back().data() + 1) == nullptr);
			for (size_t index = 0; index < capacity; ++index) {
				const std::vector<uint8_t> value(value_size, index + 1);
				REQUIRE(map.elem_update(keys[index].data() + 1, value.data()));
			}
			for (size_t index = 0; index < capacity; ++index)
				verify_value(index);
		}
	}
}

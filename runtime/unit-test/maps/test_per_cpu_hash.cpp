#include "catch2/catch_message.hpp"
#include "../common_def.hpp"
#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <bpf_map/userspace/per_cpu_hash_map.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cerrno>
#include <cstring>
#include <cstdint>
#include <linux/bpf.h>
#include <sched.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
#include <bpf_map/map_common_def.hpp>
#include "catch2/internal/catch_run_context.hpp"
#include <algorithm>
#include <array>
#include <random>

using namespace boost::interprocess;
using namespace bpftime;

static const char *SHM_NAME = "BPFTIME_PER_CPU_HASH_SHM";

TEST_CASE("four-byte key hash uses the direct integer value",
	  "[percpu-hash][hash-fast4]")
{
	const std::string shm_name =
		"BPFTIME_PER_CPU_HASH_COMPAT_SHM_" + std::to_string(getpid());
	shm_remove remover(shm_name.c_str());
	managed_shared_memory mem(create_only, shm_name.c_str(), 1 << 20);
	bytes_vec key(sizeof(uint32_t), mem.get_segment_manager());
	const std::array<std::array<uint8_t, 4>, 5> cases = { {
		{ 0x00, 0x00, 0x00, 0x00 },
		{ 0x01, 0x00, 0x00, 0x00 },
		{ 0xff, 0xff, 0xff, 0xff },
		{ 0x12, 0x34, 0x56, 0x78 },
		{ 0x78, 0x56, 0x34, 0x12 },
	} };

	for (const auto &bytes : cases) {
		std::copy(bytes.begin(), bytes.end(), key.begin());
		uint32_t expected;
		std::memcpy(&expected, bytes.data(), sizeof(expected));
		REQUIRE(per_cpu_bytes_vec_hasher{}(key) == expected);
	}
}

TEST_CASE("Test basic operations of hash map")
{
	shm_remove remover(SHM_NAME);
	managed_shared_memory mem(create_only, SHM_NAME, 20 << 20);
	uint32_t ncpu = sysconf(_SC_NPROCESSORS_ONLN);
	std::mt19937 gen;
	gen.seed(Catch::rngSeed());
	// Since this is a hash map, so we generate something randomly as keys
	std::uniform_int_distribution<uint32_t> rand(0, UINT32_MAX);
	std::vector<uint32_t> keys;
	for (int i = 0; i < 100; i++) {
		auto x = rand(gen);
		while (std::find(keys.begin(), keys.end(), x) != keys.end())
			x = rand(gen);
		keys.push_back(x);
	}

	SECTION("Test writing from helpers, and read from userspace")
	{
		per_cpu_hash_map_impl map(mem, 4, 8, 1 << 20);
		for (uint32_t j = 0; j < ncpu; j++) {
			ensure_on_certain_cpu<void>(j, [&]() {
				for (uint32_t i = 0; i < 100; i++) {
					uint64_t val =
						(((uint64_t)keys[i]) << 32) | j;
					INFO("Set index " << keys[i] << " cpu "
							  << j << " to "
							  << val);
					REQUIRE(map.elem_update(&keys[i], &val,
								0) == 0);
				}
			});
		}
		for (uint32_t i = 0; i < 100; i++) {
			uint64_t *p =
				(uint64_t *)map.elem_lookup_userspace(&keys[i]);
			for (uint32_t j = 0; j < ncpu; j++) {
				REQUIRE(p[j] ==
					((((uint64_t)keys[i]) << 32) | j));
			}
		}
	}

	SECTION("Test writing from userspace, and reading & updating from helpers")
	{
		per_cpu_hash_map_impl map(mem, 4, 8, 1 << 20);
		std::vector<uint64_t> buf(ncpu);
		for (uint32_t j = 0; j < ncpu; j++) {
			buf[j] = j;
		}
		uint32_t dummy = 0xabcdef;
		REQUIRE(map.elem_update_userspace(&dummy, buf.data(), 0) == 0);
		// Update from helpers
		for (uint32_t i = 0; i < ncpu; i++) {
			ensure_on_certain_cpu<void>(i, [&]() {
				auto valuep =
					(uint64_t *)map.elem_lookup(&dummy);
				REQUIRE(valuep != nullptr);
				auto value = *valuep;
				value |= (value << 32);
				REQUIRE(map.elem_update(&dummy, &value, 0) ==
					0);
			});
		}
		// Read from helpers
		for (uint32_t i = ncpu - 1; ~i; i--) {
			ensure_on_certain_cpu<void>(i, [&]() {
				auto valuep =
					(uint64_t *)map.elem_lookup(&dummy);
				REQUIRE(valuep != nullptr);
				auto value = *valuep;
				uint64_t a = value >> 32;
				uint64_t b = (value << 32) >> 32;
				REQUIRE(a == b);
			});
		}
	}

	SECTION("Test helper delete removes the complete per-CPU entry")
	{
		per_cpu_hash_map_impl map(mem, 4, 8, 1 << 20);
		uint32_t key = 0xabcdef;
		std::vector<uint64_t> values(ncpu);
		for (uint32_t cpu = 0; cpu < ncpu; cpu++)
			values[cpu] = cpu + 100;

		REQUIRE(map.elem_update_userspace(&key, values.data(), 0) == 0);
		REQUIRE(map.elem_lookup_userspace(&key) != nullptr);
		REQUIRE(map.elem_delete(&key) == 0);
		REQUIRE(map.elem_lookup_userspace(&key) == nullptr);
		REQUIRE(errno == ENOENT);
		REQUIRE(map.elem_delete(&key) == -1);
		REQUIRE(errno == ENOENT);
	}
}

TEST_CASE("per-cpu hash preserves arbitrary key and value bytes",
	  "[percpu-hash]")
{
	const std::string shm_name =
		"BPFTIME_PER_CPU_HASH_SIZED_SHM_" + std::to_string(getpid());
	shm_remove remover(shm_name.c_str());
	managed_shared_memory mem(create_only, shm_name.c_str(), 32 << 20);
	const int ncpu = sysconf(_SC_NPROCESSORS_ONLN);
	const int cpu = sched_getcpu();

	for (uint32_t key_size : { 1U, 4U, 16U, 17U, 64U }) {
		for (uint32_t value_size : { 1U, 8U, 24U, 257U }) {
			CAPTURE(key_size, value_size, cpu, ncpu);
			per_cpu_hash_map_impl map(mem, key_size, value_size, 16,
					       ncpu);
			std::vector<uint8_t> key(key_size);
			std::vector<uint8_t> missing_key(key_size);
			std::vector<uint8_t> initial(value_size);
			std::vector<uint8_t> updated(value_size);
			for (uint32_t i = 0; i < key_size; ++i) {
				key[i] = static_cast<uint8_t>(0x31 + i * 7);
				missing_key[i] = key[i];
			}
			missing_key[0] ^= 0xff;
			for (uint32_t i = 0; i < value_size; ++i) {
				initial[i] = static_cast<uint8_t>(i * 3 + 1);
				updated[i] = static_cast<uint8_t>(i * 5 + 9);
			}

			ensure_on_certain_cpu<void>(cpu, [&]() {
				REQUIRE(map.elem_update(key.data(), initial.data(),
							BPF_ANY) == 0);
				auto *found = static_cast<uint8_t *>(
					map.elem_lookup(key.data()));
				REQUIRE(found != nullptr);
				REQUIRE(std::memcmp(found, initial.data(),
						    value_size) == 0);
				REQUIRE(map.elem_update(key.data(), updated.data(),
							BPF_EXIST) == 0);
				found = static_cast<uint8_t *>(
					map.elem_lookup(key.data()));
				REQUIRE(found != nullptr);
				REQUIRE(std::memcmp(found, updated.data(),
						    value_size) == 0);
				REQUIRE(map.elem_lookup(missing_key.data()) ==
					nullptr);
				REQUIRE(errno == ENOENT);
			});

			auto *all_values = static_cast<uint8_t *>(
				map.elem_lookup_userspace(key.data()));
			REQUIRE(all_values != nullptr);
			for (int slot = 0; slot < ncpu; ++slot) {
				const auto *actual =
					all_values + slot * value_size;
				if (slot == cpu) {
					REQUIRE(std::memcmp(actual, updated.data(),
							    value_size) == 0);
				} else {
					REQUIRE(std::all_of(
						actual, actual + value_size,
						[](uint8_t byte) {
							return byte == 0;
						}));
				}
			}
		}
	}
}

TEST_CASE("four-byte per-cpu hash preserves userspace map semantics",
	  "[percpu-hash][uint32-key]")
{
	const std::string shm_name =
		"BPFTIME_PER_CPU_HASH_UINT32_SHM_" + std::to_string(getpid());
	shm_remove remover(shm_name.c_str());
	managed_shared_memory mem(create_only, shm_name.c_str(), 4 << 20);
	const int ncpu = sysconf(_SC_NPROCESSORS_ONLN);
	per_cpu_hash_map_impl map(mem, sizeof(uint32_t), sizeof(uint64_t), 2,
				       ncpu);
	std::vector<uint64_t> values(ncpu);
	for (int cpu = 0; cpu < ncpu; ++cpu)
		values[cpu] = 1000 + cpu;

	uint32_t key1 = 0x12345678;
	uint32_t key2 = 0xabcdef01;
	uint32_t missing = 0xfeedface;
	REQUIRE(map.elem_update_userspace(&key1, values.data(), BPF_NOEXIST) ==
		0);
	REQUIRE(map.elem_update_userspace(&key1, values.data(), BPF_NOEXIST) ==
		-1);
	REQUIRE(errno == EEXIST);
	REQUIRE(map.elem_update_userspace(&missing, values.data(), BPF_EXIST) ==
		-1);
	REQUIRE(errno == ENOENT);
	REQUIRE(map.elem_update_userspace(&key2, values.data(), BPF_NOEXIST) ==
		0);
	REQUIRE(map.elem_update_userspace(&missing, values.data(), BPF_NOEXIST) ==
		-1);
	REQUIRE(errno == E2BIG);

	std::vector<uint32_t> iterated;
	uint32_t current;
	REQUIRE(map.map_get_next_key(nullptr, &current) == 0);
	iterated.push_back(current);
	while (map.map_get_next_key(&current, &current) == 0)
		iterated.push_back(current);
	REQUIRE(errno == ENOENT);
	std::sort(iterated.begin(), iterated.end());
	std::array<uint32_t, 2> expected{ key1, key2 };
	std::sort(expected.begin(), expected.end());
	REQUIRE(iterated == std::vector<uint32_t>(expected.begin(),
						  expected.end()));

	std::vector<uint64_t> copied(ncpu);
	REQUIRE(map.lookup_and_delete_userspace(&key1, copied.data()) == 0);
	REQUIRE(copied == values);
	REQUIRE(map.elem_lookup_userspace(&key1) == nullptr);
	REQUIRE(errno == ENOENT);
	REQUIRE(map.elem_delete_userspace(&key2) == 0);
	REQUIRE(map.map_get_next_key(nullptr, &current) == -1);
	REQUIRE(errno == ENOENT);
}

TEST_CASE("four-byte per-cpu hash reuses value slots without stale data",
	  "[percpu-hash][uint32-key][slot-pool]")
{
	const std::string shm_name =
		"BPFTIME_PER_CPU_HASH_SLOT_SHM_" + std::to_string(getpid());
	shm_remove remover(shm_name.c_str());
	managed_shared_memory mem(create_only, shm_name.c_str(), 4 << 20);
	const int ncpu = sysconf(_SC_NPROCESSORS_ONLN);
	const int cpu = sched_getcpu();
	per_cpu_hash_map_impl map(mem, sizeof(uint32_t), sizeof(uint64_t), 2,
				       ncpu);
	std::vector<uint64_t> old_values(ncpu);
	for (int slot = 0; slot < ncpu; ++slot)
		old_values[slot] = 1000 + slot;

	uint32_t key = 1;
	REQUIRE(map.elem_update_userspace(&key, old_values.data(), BPF_ANY) ==
		0);
	REQUIRE(map.elem_delete(&key) == 0);

	key = 2;
	const uint64_t new_value = 0xabcdef0123456789ULL;
	ensure_on_certain_cpu<void>(cpu, [&]() {
		REQUIRE(map.elem_update(&key, &new_value, BPF_ANY) == 0);
	});
	auto *values = static_cast<uint64_t *>(map.elem_lookup_userspace(&key));
	REQUIRE(values != nullptr);
	for (int slot = 0; slot < ncpu; ++slot) {
		if (slot == cpu)
			REQUIRE(values[slot] == new_value);
		else
			REQUIRE(values[slot] == 0);
	}

	REQUIRE(map.elem_delete(&key) == 0);
	const auto steady_free_memory = mem.get_free_memory();
	for (uint32_t iteration = 0; iteration < 2048; ++iteration) {
		key = 100 + iteration;
		REQUIRE(map.elem_update_userspace(&key, old_values.data(),
						  BPF_ANY) == 0);
		REQUIRE(map.elem_delete_userspace(&key) == 0);
	}
	REQUIRE(mem.get_free_memory() == steady_free_memory);
}

TEST_CASE("four-byte per-cpu hash keeps values stable across pool growth",
	  "[percpu-hash][uint32-key][slot-pool]")
{
	const std::string shm_name =
		"BPFTIME_PER_CPU_HASH_GROW_SHM_" + std::to_string(getpid());
	shm_remove remover(shm_name.c_str());
	managed_shared_memory mem(create_only, shm_name.c_str(), 4 << 20);
	constexpr int ncpu = 4;
	constexpr uint32_t entry_count = 513;
	per_cpu_hash_map_impl map(mem, sizeof(uint32_t), sizeof(uint64_t),
				       entry_count, ncpu);

	for (uint32_t key = 0; key < entry_count; ++key) {
		std::array<uint64_t, ncpu> values{};
		for (int cpu = 0; cpu < ncpu; ++cpu)
			values[cpu] = static_cast<uint64_t>(key) * 100 + cpu;
		REQUIRE(map.elem_update_userspace(&key, values.data(),
						  BPF_NOEXIST) == 0);
	}

	for (uint32_t key : { 0U, 1U, 255U, 256U, 511U, 512U }) {
		auto *values =
			static_cast<uint64_t *>(map.elem_lookup_userspace(&key));
		REQUIRE(values != nullptr);
		for (int cpu = 0; cpu < ncpu; ++cpu)
			REQUIRE(values[cpu] ==
				static_cast<uint64_t>(key) * 100 + cpu);
	}
}

#include "catch2/catch_message.hpp"
#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <bpf_map/userspace/per_cpu_array_map.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <linux/bpf.h>
#include <limits>
#include "bpf_map/userspace/array_map.hpp"
#include <string>
#include <thread>
#include <sched.h>
#include <unistd.h>
#include <vector>
#include <bpf_map/map_common_def.hpp>
#include "../common_def.hpp"

using namespace boost::interprocess;
using namespace bpftime;

static const char *SHM_NAME = "_PER_CPU_ARRAY_SHM";

TEST_CASE("Per-CPU array helper errors preserve stored values", "[percpu-array]")
{
	const auto name = "_PER_CPU_ARRAY_ERRORS_" + std::to_string(getpid());
	shm_remove remover(name.c_str());
	managed_shared_memory mem(create_only, name.c_str(), 1 << 20);
	per_cpu_array_map_impl map(mem, sizeof(uint64_t), 2);
	const uint32_t key = 1, invalid_key = 2;
	const uint64_t value = 99;
	std::vector<uint64_t> initial(sysconf(_SC_NPROCESSORS_ONLN), 42);
	REQUIRE(map.elem_update_userspace(&key, initial.data(), BPF_ANY) == 0);

	REQUIRE(map.elem_lookup(nullptr) == nullptr);
	REQUIRE(errno == ENOENT);
	REQUIRE(map.elem_lookup(&invalid_key) == nullptr);
	REQUIRE(errno == ENOENT);
	REQUIRE(map.elem_update(nullptr, &value, BPF_ANY) == -1);
	REQUIRE(errno == ENOENT);
	REQUIRE(map.elem_update(&invalid_key, &value, BPF_ANY) == -1);
	REQUIRE(errno == E2BIG);
	REQUIRE(map.elem_update(&key, &value, BPF_NOEXIST) == -1);
	REQUIRE(errno == EEXIST);
	REQUIRE(map.elem_update(&key, &value, 3) == -1);
	REQUIRE(errno == EINVAL);
	REQUIRE(map.elem_delete(&key) == -1);
	REQUIRE(errno == EINVAL);
	const auto *actual = static_cast<const uint64_t *>(
		map.elem_lookup_userspace(&key));
	REQUIRE(actual != nullptr);
	REQUIRE(std::equal(initial.begin(), initial.end(), actual));
	REQUIRE(map.elem_update(&key, &value, BPF_EXIST) == 0);
	REQUIRE(map.elem_update(&key, &value, BPF_ANY) == 0);
}

TEST_CASE("Per-CPU array helpers preserve CPU slots under concurrent writes",
	  "[percpu-array]")
{
	cpu_set_t allowed;
	REQUIRE(sched_getaffinity(0, sizeof(allowed), &allowed) == 0);
	std::vector<int> cpus;
	for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
		if (CPU_ISSET(cpu, &allowed))
			cpus.push_back(cpu);
	REQUIRE_FALSE(cpus.empty());
	const uint32_t ncpu = cpus.back() + 1;
	const auto name = "_PER_CPU_ARRAY_THREADS_" + std::to_string(getpid());
	shm_remove remover(name.c_str());
	managed_shared_memory mem(create_only, name.c_str(), 4 << 20);

	for (const uint32_t value_size : {8u, 24u, 256u}) {
		INFO("value_size=" << value_size);
		per_cpu_array_map_impl map(mem, value_size, 2, ncpu);
		std::atomic<bool> ok{true};
		std::vector<std::thread> workers;
		for (const int cpu : cpus) {
			workers.emplace_back([&, cpu] {
				cpu_set_t mask;
				CPU_ZERO(&mask);
				CPU_SET(cpu, &mask);
				if (sched_setaffinity(0, sizeof(mask), &mask) != 0) {
					ok.store(false);
					return;
				}
				std::vector<uint8_t> value(value_size);
				const uint32_t key = 1;
				for (unsigned iter = 0; iter < 1000; ++iter) {
					std::fill(value.begin(), value.end(),
						  static_cast<uint8_t>(cpu + iter));
					if (map.elem_update(&key, value.data(), BPF_ANY) != 0) {
						ok.store(false);
						return;
					}
					const auto *actual = static_cast<const uint8_t *>(
						map.elem_lookup(&key));
					if (!actual || !std::equal(value.begin(), value.end(), actual)) {
						ok.store(false);
						return;
					}
				}
			});
		}
		for (auto &worker : workers)
			worker.join();
		REQUIRE(ok.load());
		const uint32_t key = 1, untouched_key = 0;
		const auto *actual = static_cast<const uint8_t *>(
			map.elem_lookup_userspace(&key));
		REQUIRE(actual != nullptr);
		for (const int cpu : cpus)
			for (uint32_t byte = 0; byte < value_size; ++byte)
				REQUIRE(actual[cpu * value_size + byte] ==
					static_cast<uint8_t>(cpu + 999));
		const auto *untouched = static_cast<const uint8_t *>(
			map.elem_lookup_userspace(&untouched_key));
		REQUIRE(untouched != nullptr);
		REQUIRE(std::all_of(untouched, untouched + ncpu * value_size,
				    [](uint8_t byte) { return byte == 0; }));
	}
}

TEST_CASE("Per-CPU array production lookup follows CPU affinity changes",
	  "[percpu-array][lookup-affinity]")
{
	cpu_set_t allowed;
	REQUIRE(sched_getaffinity(0, sizeof(allowed), &allowed) == 0);
	struct affinity_restore {
		cpu_set_t mask;
		~affinity_restore()
		{
			sched_setaffinity(0, sizeof(mask), &mask);
		}
	} restore{allowed};
	std::vector<int> cpus;
	for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
		if (CPU_ISSET(cpu, &allowed))
			cpus.push_back(cpu);
	REQUIRE_FALSE(cpus.empty());
	const int ncpu = cpus.back() + 1;
	const auto name = "_PER_CPU_ARRAY_LOOKUP_" + std::to_string(getpid());
	shm_remove remover(name.c_str());
	managed_shared_memory mem(create_only, name.c_str(), 4 << 20);

	for (uint32_t value_size : { 1U, 8U, 24U, 257U }) {
		CAPTURE(value_size);
		per_cpu_array_map_impl map(mem, value_size, 4, ncpu);
		for (uint32_t key : { 0U, 3U }) {
			std::vector<uint8_t> values(ncpu * value_size);
			for (int cpu = 0; cpu < ncpu; ++cpu)
				for (uint32_t byte = 0; byte < value_size; ++byte)
					values[cpu * value_size + byte] =
						static_cast<uint8_t>(key * 17 + cpu * 29 + byte);
			REQUIRE(map.elem_update_userspace(&key, values.data(), BPF_ANY) == 0);
		}
		// Change the executing CPU between calls on the same map.
		for (int cpu : cpus) {
			CAPTURE(cpu);
			cpu_set_t selected;
			CPU_ZERO(&selected);
			CPU_SET(cpu, &selected);
			REQUIRE(sched_setaffinity(0, sizeof(selected), &selected) == 0);
			for (uint32_t key : { 0U, 3U }) {
				const auto *value = static_cast<const uint8_t *>(
					map.elem_lookup_base(&key));
				REQUIRE(value != nullptr);
				for (uint32_t byte = 0; byte < value_size; ++byte)
					REQUIRE(value[byte] ==
						static_cast<uint8_t>(key * 17 + cpu * 29 + byte));
			}
			for (uint32_t key : { 4U, std::numeric_limits<uint32_t>::max() }) {
				REQUIRE(map.elem_lookup_base(&key) == nullptr);
				REQUIRE(errno == ENOENT);
			}
			REQUIRE(map.elem_lookup_base(nullptr) == nullptr);
			REQUIRE(errno == ENOENT);
		}
	}
}

TEST_CASE("Ordinary array lookup preserves bounds and errno on its cold path",
	  "[array-lookup-cold]")
{
	const auto name = "_ARRAY_LOOKUP_COLD_" + std::to_string(getpid());
	shm_remove remover(name.c_str());
	managed_shared_memory mem(create_only, name.c_str(), 4 << 20);
	for (uint32_t value_size : { 1U, 8U, 24U, 257U }) {
		CAPTURE(value_size);
		array_map_impl map(mem, value_size, 4);
		for (uint32_t key : { 0U, 3U }) {
			std::vector<uint8_t> expected(value_size);
			for (uint32_t byte = 0; byte < value_size; ++byte)
				expected[byte] = static_cast<uint8_t>(key * 17 + byte);
			REQUIRE(map.elem_update(&key, expected.data(), BPF_ANY) == 0);
			errno = EDOM;
			auto *value = static_cast<uint8_t *>(map.elem_lookup(&key));
			REQUIRE(value != nullptr);
			REQUIRE(errno == EDOM);
			for (uint32_t byte = 0; byte < value_size; ++byte)
				REQUIRE(value[byte] == expected[byte]);
			value[0] ^= 0xff;
			REQUIRE(static_cast<uint8_t *>(map.elem_lookup(&key))[0] ==
				static_cast<uint8_t>(expected[0] ^ 0xff));
		}
		for (uint32_t key : { 4U, std::numeric_limits<uint32_t>::max() }) {
			errno = EDOM;
			REQUIRE(map.elem_lookup(&key) == nullptr);
			REQUIRE(errno == ENOENT);
		}
		uint32_t key = 0;
		REQUIRE(map.elem_lookup(&key) != nullptr);
		REQUIRE(errno == ENOENT);
	}
}

TEST_CASE("Ordinary array address preparation preserves bounds and byte strides",
	  "[array-lookup-cold][array-load-order]")
{
	const auto name = "_ARRAY_LOAD_ORDER_" + std::to_string(getpid());
	shm_remove remover(name.c_str());
	managed_shared_memory mem(create_only, name.c_str(), 4 << 20);
	for (uint32_t value_size : { 1U, 3U, 8U, 24U, 257U }) {
		for (uint32_t capacity : { 1U, 7U, 65U, 1024U }) {
			CAPTURE(value_size, capacity);
			array_map_impl map(mem, value_size, capacity);
			uint32_t first = 0;
			auto *base = static_cast<uint8_t *>(map.elem_lookup(&first));
			REQUIRE(base != nullptr);
			for (uint32_t key : { 0U, capacity / 2, capacity - 1 }) {
				std::vector<uint8_t> expected(value_size);
				for (uint32_t byte = 0; byte < value_size; ++byte)
					expected[byte] = static_cast<uint8_t>(key * 17 + byte);
				REQUIRE(map.elem_update(&key, expected.data(), BPF_ANY) == 0);
				errno = EDOM;
				auto *value = static_cast<uint8_t *>(map.elem_lookup(&key));
				REQUIRE(value == base + key * value_size);
				REQUIRE(errno == EDOM);
				REQUIRE(std::equal(expected.begin(), expected.end(), value));
			}
			for (uint32_t key : { capacity, capacity + 1,
					     std::numeric_limits<uint32_t>::max() }) {
				errno = EDOM;
				REQUIRE(map.elem_lookup(&key) == nullptr);
				REQUIRE(errno == ENOENT);
			}
		}
	}
}

TEST_CASE("Test basic operations of array map")
{
	shm_remove remover(SHM_NAME);
	managed_shared_memory mem(create_only, SHM_NAME, 20 << 20);
	uint32_t ncpu = sysconf(_SC_NPROCESSORS_ONLN);

	SECTION("Test writing from helpers, and read from userspace")
	{
		per_cpu_array_map_impl map(mem, 8, 10);
		for (uint32_t j = 0; j < ncpu; j++) {
			ensure_on_certain_cpu<void>(j, [&]() {
				for (uint32_t i = 0; i < 10; i++) {
					uint64_t val =
						(((uint64_t)i) << 32) | j;
					INFO("Set index " << i << " cpu " << j
							  << " to " << val);
					REQUIRE(map.elem_update(&i, &val, 0) ==
						0);
				}
			});
		}
		for (uint32_t i = 0; i < 10; i++) {
			uint64_t *p = (uint64_t *)map.elem_lookup_userspace(&i);
			for (uint32_t j = 0; j < ncpu; j++) {
				REQUIRE(p[j] == ((((uint64_t)i) << 32) | j));
			}
		}
	}

	SECTION("Test writing from userspace, and reading & updating from helpers")
	{
		per_cpu_array_map_impl map(mem, 8, 1);
		std::vector<uint64_t> buf(ncpu);
		for (uint32_t j = 0; j < ncpu; j++) {
			buf[j] = j;
		}
		uint32_t zero = 0;
		REQUIRE(map.elem_update_userspace(&zero, buf.data(), 0) == 0);
		// Update from helpers
		for (uint32_t i = 0; i < ncpu; i++) {
			ensure_on_certain_cpu<void>(i, [&]() {
				auto valuep =
					(uint64_t *)map.elem_lookup(&zero);
				REQUIRE(valuep != nullptr);
				auto value = *valuep;
				value |= (value << 32);
				REQUIRE(map.elem_update(&zero, &value, 0) == 0);
			});
		}
		// Read from helpers
		for (uint32_t i = ncpu - 1; ~i; i--) {
			ensure_on_certain_cpu<void>(i, [&]() {
				auto valuep =
					(uint64_t *)map.elem_lookup(&zero);
				REQUIRE(valuep != nullptr);
				auto value = *valuep;
				uint64_t a = value >> 32;
				uint64_t b = (value << 32) >> 32;
				REQUIRE(a == b);
			});
		}
	}
}

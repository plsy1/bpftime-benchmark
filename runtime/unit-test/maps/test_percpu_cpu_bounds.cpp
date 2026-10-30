// Per-CPU maps index their values with sched_getcpu(). The CPU count is
// fixed at creation (it used to be the CPUs online then); a CPU numbered at
// or above it read and wrote past the values, and -1 indexed before them.
#include <boost/interprocess/managed_shared_memory.hpp>
#include <bpf_map/userspace/per_cpu_array_map.hpp>
#include <bpf_map/userspace/per_cpu_hash_map.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cerrno>
#include <cstdint>
#include <fstream>
#include <sched.h>
#include <string>
#include <unistd.h>
#include "platform_utils.hpp"

using namespace boost::interprocess;
using namespace bpftime;

namespace
{
struct pinned_to {
	cpu_set_t old;
	bool ok = false;
	explicit pinned_to(int cpu)
	{
		cpu_set_t set;
		CPU_ZERO(&set);
		CPU_SET(cpu, &set);
		ok = sched_getaffinity(0, sizeof(old), &old) == 0 &&
		     sched_setaffinity(0, sizeof(set), &set) == 0;
	}
	~pinned_to()
	{
		if (ok)
			sched_setaffinity(0, sizeof(old), &old);
	}
};
} // namespace

TEST_CASE("Per-CPU maps reject a CPU beyond their count",
	  "[percpu-hash][percpu-array][cpu-bounds]")
{
	if (sysconf(_SC_NPROCESSORS_ONLN) < 2)
		SKIP("needs two CPUs");
	const std::string name = "bpftime_percpu_bounds_" +
				 std::to_string(getpid());
	shared_memory_object::remove(name.c_str());
	managed_shared_memory segment(create_only, name.c_str(), 4 << 20);
	// Created for one CPU; this thread runs on CPU 1.
	per_cpu_array_map_impl array(segment, 8, 4, 1);
	per_cpu_hash_map_impl hash4(segment, 4, 8, 16, 1);
	per_cpu_hash_map_impl hash8(segment, 8, 8, 16, 1);
	pinned_to cpu1(1);
	REQUIRE(cpu1.ok);
	REQUIRE(my_sched_getcpu() == 1);
	uint32_t key4 = 0;
	uint64_t key8 = 0, value = 7;

	errno = 0;
	CHECK(array.elem_lookup_base(&key4) == nullptr);
	CHECK(errno == ENOENT);
	CHECK(array.elem_update_base(&key4, &value, 0) == -1);
	CHECK(errno == EINVAL);
	for (auto *hash : { &hash4, &hash8 }) {
		const void *key = hash == &hash4 ? (const void *)&key4 :
						   (const void *)&key8;
		errno = 0;
		CHECK(hash->elem_update(key, &value, 0) == -1);
		CHECK(errno == EINVAL);
		CHECK(hash->elem_lookup(key) == nullptr);
		CHECK(hash->elem_delete(key) == -1);
		CHECK(errno == EINVAL);
	}
	shared_memory_object::remove(name.c_str());
}

TEST_CASE("Possible CPUs are counted as libbpf does", "[cpu-bounds]")
{
	std::ifstream f("/sys/devices/system/cpu/possible");
	std::string text;
	REQUIRE(std::getline(f, text));
	// The highest number in "0-5" or "0,2-3".
	int highest = -1, n = 0;
	bool in_number = false;
	for (char c : text) {
		if (c >= '0' && c <= '9') {
			n = (in_number ? n * 10 : 0) + (c - '0');
			in_number = true;
		} else {
			if (in_number && n > highest)
				highest = n;
			in_number = false;
		}
	}
	if (in_number && n > highest)
		highest = n;
	REQUIRE(bpftime_num_possible_cpus() == highest + 1);
}

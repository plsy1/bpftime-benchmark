// libbpf reads and writes per-CPU values through bpf() as one slot per
// possible CPU, each rounded up to 8 bytes (the kernel's layout). bpftime
// copied value_size * online CPUs packed: with a 4-byte counter a tool read
// CPU 1's value from CPU 0's padding.
#include <boost/interprocess/managed_shared_memory.hpp>
#include <bpf_map/userspace/per_cpu_array_map.hpp>
#include <bpf_map/userspace/per_cpu_hash_map.hpp>
#include <bpftime_shm.hpp>
#include <bpftime_shm_internal.hpp>
#include <catch2/catch_test_macros.hpp>
#include <linux/bpf.h>
#include <cstdint>
#include <cstring>
#include <sched.h>
#include <string>
#include <unistd.h>
#include <vector>
#include "platform_utils.hpp"

using namespace boost::interprocess;
using namespace bpftime;

namespace
{
// The value a CPU holds in these tests: byte i of CPU c is 16 * c + i + 1.
std::vector<uint8_t> userspace_values(uint32_t value_size, int ncpu)
{
	const size_t stride = (value_size + 7) & ~7u;
	std::vector<uint8_t> out(stride * ncpu, 0xee);
	for (int c = 0; c < ncpu; c++)
		for (uint32_t i = 0; i < value_size; i++)
			out[c * stride + i] = (uint8_t)(16 * c + i + 1);
	return out;
}

bool matches(const uint8_t *got, uint32_t value_size, int ncpu)
{
	const size_t stride = (value_size + 7) & ~7u;
	for (int c = 0; c < ncpu; c++)
		for (size_t i = 0; i < stride; i++)
			if (got[c * stride + i] !=
			    (i < value_size ? (uint8_t)(16 * c + i + 1) : 0))
				return false;
	return true;
}

// The program path on CPU `cpu` sees that CPU's value.
template <class Map>
bool program_sees(Map &map, const void *key, uint32_t value_size, int cpu)
{
	cpu_set_t old, set;
	CPU_ZERO(&set);
	CPU_SET(cpu, &set);
	if (sched_getaffinity(0, sizeof(old), &old) != 0 ||
	    sched_setaffinity(0, sizeof(set), &set) != 0)
		return false;
	auto *p = static_cast<const uint8_t *>(map.elem_lookup(key));
	bool ok = p != nullptr;
	for (uint32_t i = 0; ok && i < value_size; i++)
		ok = p[i] == (uint8_t)(16 * cpu + i + 1);
	sched_setaffinity(0, sizeof(old), &old);
	return ok;
}
} // namespace

TEST_CASE("Per-CPU values use libbpf's layout through bpf()",
	  "[percpu-hash][percpu-array][percpu-layout]")
{
	const std::string name =
		"bpftime_percpu_layout_" + std::to_string(getpid());
	shared_memory_object::remove(name.c_str());
	managed_shared_memory segment(create_only, name.c_str(), 8 << 20);
	const int ncpu = bpftime_num_possible_cpus();
	for (uint32_t value_size : { 4u, 8u, 12u }) {
		INFO("value size " << value_size);
		auto in = userspace_values(value_size, ncpu);
		uint32_t key4 = 1;
		uint64_t key8 = 1;

		per_cpu_array_map_impl array(segment, value_size, 4);
		REQUIRE(array.elem_update_userspace(&key4, in.data(), 0) == 0);
		REQUIRE(matches(static_cast<const uint8_t *>(
					array.elem_lookup_userspace(&key4)),
				value_size, ncpu));
		CHECK(program_sees(array, &key4, value_size, ncpu - 1));

		per_cpu_hash_map_impl hash4(segment, 4, value_size, 4);
		per_cpu_hash_map_impl hash8(segment, 8, value_size, 4);
		for (auto *hash : { &hash4, &hash8 }) {
			const void *key = hash == &hash4 ? (const void *)&key4 :
							   (const void *)&key8;
			REQUIRE(hash->elem_update_userspace(key, in.data(), 0) ==
				0);
			REQUIRE(matches(static_cast<const uint8_t *>(
						hash->elem_lookup_userspace(key)),
					value_size, ncpu));
			CHECK(program_sees(*hash, key, value_size, ncpu - 1));
			std::vector<uint8_t> out(in.size(), 0xcc);
			REQUIRE(hash->lookup_and_delete_userspace(key,
								  out.data()) ==
				0);
			REQUIRE(matches(out.data(), value_size, ncpu));
		}
	}
	shared_memory_object::remove(name.c_str());
}

TEST_CASE("Per-CPU values of a possible mask with holes pack its CPUs",
	  "[percpu-hash][percpu-array][percpu-layout]")
{
	// "0,2-3": libbpf allocates three slots, for CPUs 0, 2 and 3.
	if (sysconf(_SC_NPROCESSORS_ONLN) < 4)
		SKIP("needs CPUs 2 and 3");
	const auto saved = bpftime_possible_cpu_ids();
	bpftime_set_possible_cpus_for_test({ 0, 2, 3 });
	const std::string name =
		"bpftime_percpu_holes_" + std::to_string(getpid());
	shared_memory_object::remove(name.c_str());
	{
		managed_shared_memory segment(create_only, name.c_str(),
					      8 << 20);
		for (uint32_t value_size : { 4u, 8u }) {
			INFO("value size " << value_size);
			const size_t stride = (value_size + 7) & ~7u;
			// Slot i: CPU ids[i]; byte j is 16 * ids[i] + j + 1.
			const int ids[] = { 0, 2, 3 };
			std::vector<uint8_t> in(stride * 3, 0);
			for (int i = 0; i < 3; i++)
				for (uint32_t j = 0; j < value_size; j++)
					in[i * stride + j] =
						(uint8_t)(16 * ids[i] + j + 1);
			uint32_t key4 = 1;
			uint64_t key8 = 1;
			per_cpu_array_map_impl array(segment, value_size, 4);
			per_cpu_hash_map_impl hash8(segment, 8, value_size, 4);
			REQUIRE(array.elem_update_userspace(&key4, in.data(),
							    0) == 0);
			REQUIRE(hash8.elem_update_userspace(&key8, in.data(),
							    0) == 0);
			for (int cpu : { 2, 3 }) {
				CHECK(program_sees(array, &key4, value_size,
						   cpu));
				CHECK(program_sees(hash8, &key8, value_size,
						   cpu));
			}
			for (const void *got :
			     { array.elem_lookup_userspace(&key4),
			       hash8.elem_lookup_userspace(&key8) }) {
				REQUIRE(got != nullptr);
				CHECK(std::memcmp(got, in.data(), in.size()) ==
				      0);
			}
		}
	}
	shared_memory_object::remove(name.c_str());
	bpftime_set_possible_cpus_for_test(saved);
}

TEST_CASE("bpf() copies per-CPU values in libbpf's size",
	  "[percpu-layout]")
{
	const std::string name =
		"bpftime-percpu-layout-shm-" + std::to_string(getpid());
	const char *had = getenv("BPFTIME_GLOBAL_SHM_NAME");
	const std::string previous = had ? had : "";
	bpftime_destroy_global_shm();
	REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
	bpftime_initialize_global_shm(shm_open_type::SHM_REMOVE_AND_CREATE);
	const size_t ncpu = bpftime_num_possible_cpus();
	REQUIRE(bpftime_maps_create(100, "percpu_array_layout",
				    { .type = BPF_MAP_TYPE_PERCPU_ARRAY,
				      .key_size = 4,
				      .value_size = 4,
				      .max_ents = 4 }) == 100);
	REQUIRE(bpftime_maps_create(101, "percpu_hash_layout",
				    { .type = BPF_MAP_TYPE_PERCPU_HASH,
				      .key_size = 8,
				      .value_size = 12,
				      .max_ents = 4 }) == 101);
	CHECK(bpftime_map_value_size_from_syscall(100) == 8 * ncpu);
	CHECK(bpftime_map_value_size_from_syscall(101) == 16 * ncpu);
	shm_holder.global_shared_memory.reset_server_state();
	bpftime_destroy_global_shm();
	shared_memory_object::remove(name.c_str());
	bpftime_initialize_global_shm(shm_open_type::SHM_NO_CREATE);
	if (had)
		setenv("BPFTIME_GLOBAL_SHM_NAME", previous.c_str(), 1);
	else
		unsetenv("BPFTIME_GLOBAL_SHM_NAME");
}

TEST_CASE("bpf() lookup and delete takes the key of hash maps",
	  "[percpu-layout][lookup-and-delete]")
{
	const std::string name =
		"bpftime-lookup-delete-shm-" + std::to_string(getpid());
	const char *had = getenv("BPFTIME_GLOBAL_SHM_NAME");
	const std::string previous = had ? had : "";
	bpftime_destroy_global_shm();
	REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
	bpftime_initialize_global_shm(shm_open_type::SHM_REMOVE_AND_CREATE);
	const size_t ncpu = bpftime_num_possible_cpus();
	struct {
		int fd;
		int type;
		size_t value_bytes;
	} maps[] = { { 100, BPF_MAP_TYPE_HASH, 8 },
		     { 101, BPF_MAP_TYPE_PERCPU_HASH, 8 * ncpu },
		     { 102, BPF_MAP_TYPE_LRU_HASH, 8 } };
	for (auto &m : maps) {
		INFO("map type " << m.type);
		REQUIRE(bpftime_maps_create(m.fd, "lookup_delete",
					    { .type = m.type,
					      .key_size = 8,
					      .value_size = 8,
					      .max_ents = 16 }) == m.fd);
		std::vector<uint8_t> in(m.value_bytes), out(m.value_bytes, 0);
		for (size_t i = 0; i < in.size(); i++)
			in[i] = (uint8_t)(i + 1);
		const uint64_t key = 7, other = 8;
		REQUIRE(bpftime_map_update_elem(m.fd, &key, in.data(), 0) == 0);
		REQUIRE(bpftime_map_update_elem(m.fd, &other, in.data(), 0) ==
			0);
		REQUIRE(bpftime_map_lookup_and_delete_elem(m.fd, &key,
							   out.data()) == 0);
		CHECK(out == in);
		CHECK(bpftime_map_lookup_elem(m.fd, &key) == nullptr);
		CHECK(bpftime_map_lookup_elem(m.fd, &other) != nullptr);
		errno = 0;
		CHECK(bpftime_map_lookup_and_delete_elem(m.fd, &key,
							 out.data()) == -1);
		CHECK(errno == ENOENT);
	}
	// Queues pop; other types have no lookup and delete.
	REQUIRE(bpftime_maps_create(103, "lookup_delete_queue",
				    { .type = BPF_MAP_TYPE_QUEUE,
				      .key_size = 0,
				      .value_size = 8,
				      .max_ents = 4 }) == 103);
	const uint64_t pushed = 42;
	uint64_t popped = 0;
	REQUIRE(bpftime_map_push_elem(103, &pushed, 0) == 0);
	REQUIRE(bpftime_map_lookup_and_delete_elem(103, nullptr, &popped) ==
		0);
	CHECK(popped == 42);
	REQUIRE(bpftime_maps_create(104, "lookup_delete_array",
				    { .type = BPF_MAP_TYPE_ARRAY,
				      .key_size = 4,
				      .value_size = 8,
				      .max_ents = 4 }) == 104);
	const uint32_t index = 1;
	errno = 0;
	CHECK(bpftime_map_lookup_and_delete_elem(104, &index, &popped) == -1);
	CHECK(errno == ENOTSUP);
	shm_holder.global_shared_memory.reset_server_state();
	bpftime_destroy_global_shm();
	shared_memory_object::remove(name.c_str());
	bpftime_initialize_global_shm(shm_open_type::SHM_NO_CREATE);
	if (had)
		setenv("BPFTIME_GLOBAL_SHM_NAME", previous.c_str(), 1);
	else
		unsetenv("BPFTIME_GLOBAL_SHM_NAME");
}

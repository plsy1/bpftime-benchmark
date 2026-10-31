// Map creation checks sizes as the kernel does: a size computation that
// wrapped used to allocate a small buffer that accesses then overran (a hash
// map of 2^32 entries of 2 GiB values got 97 MiB, its first lookup SIGSEGV;
// a per-CPU array of 65536 x 65536 bytes got 0), and degenerate sizes were
// accepted (a 0-byte key matched every key).
#include <boost/interprocess/managed_shared_memory.hpp>
#include <bpf_map/userspace/per_cpu_array_map.hpp>
#include <bpftime_shm.hpp>
#include <bpftime_shm_internal.hpp>
#include <catch2/catch_test_macros.hpp>
#include <linux/bpf.h>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <unistd.h>

using namespace boost::interprocess;
using namespace bpftime;

TEST_CASE("Map creation rejects sizes the kernel rejects", "[map-create]")
{
	const std::string name =
		"bpftime-map-create-checks-" + std::to_string(getpid());
	const char *had = getenv("BPFTIME_GLOBAL_SHM_NAME");
	const std::string previous = had ? had : "";
	bpftime_destroy_global_shm();
	REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
	bpftime_initialize_global_shm(shm_open_type::SHM_REMOVE_AND_CREATE);
	struct {
		const char *what;
		int type;
		uint32_t key_size, value_size, max_entries;
		int error;
	} cases[] = {
		{ "hash size wraps", BPF_MAP_TYPE_HASH, 4, 2147506256u,
		  4294922064u, E2BIG },
		{ "per-CPU array larger than the segment",
		  BPF_MAP_TYPE_PERCPU_ARRAY, 4, 32768, 1u << 20, ENOMEM },
		{ "array value over 4 MiB", BPF_MAP_TYPE_ARRAY, 4,
		  (4u << 20) + 1, 1, E2BIG },
		{ "per-CPU value over 32 KiB", BPF_MAP_TYPE_PERCPU_ARRAY, 4,
		  65536, 4, E2BIG },
		{ "hash key over 512 bytes", BPF_MAP_TYPE_HASH, 513, 8, 4,
		  E2BIG },
		{ "hash key of 0 bytes", BPF_MAP_TYPE_HASH, 0, 8, 4, EINVAL },
		{ "hash value of 0 bytes", BPF_MAP_TYPE_HASH, 4, 0, 4, EINVAL },
		{ "hash of 0 entries", BPF_MAP_TYPE_HASH, 4, 8, 0, EINVAL },
		{ "array of 0 entries", BPF_MAP_TYPE_ARRAY, 4, 8, 0, EINVAL },
		{ "queue value of 0 bytes", BPF_MAP_TYPE_QUEUE, 0, 0, 4,
		  EINVAL },
	};
	for (auto &c : cases) {
		INFO(c.what);
		errno = 0;
		CHECK(bpftime_maps_create(100, "checked",
					  { .type = c.type,
					    .key_size = c.key_size,
					    .value_size = c.value_size,
					    .max_ents = c.max_entries }) == -1);
		CHECK(errno == c.error);
		// The descriptor is free again.
		REQUIRE(bpftime_maps_create(100, "checked_ok",
					    { .type = BPF_MAP_TYPE_HASH,
					      .key_size = 4,
					      .value_size = 8,
					      .max_ents = 4 }) == 100);
		bpftime_close(100);
	}
	shm_holder.global_shared_memory.reset_server_state();
	bpftime_destroy_global_shm();
	shared_memory_object::remove(name.c_str());
	bpftime_initialize_global_shm(shm_open_type::SHM_NO_CREATE);
	if (had)
		setenv("BPFTIME_GLOBAL_SHM_NAME", previous.c_str(), 1);
	else
		unsetenv("BPFTIME_GLOBAL_SHM_NAME");
}

TEST_CASE("Per-CPU array size is computed in 64 bits", "[map-create]")
{
	const std::string name =
		"bpftime_percpu_array_size_" + std::to_string(getpid());
	shared_memory_object::remove(name.c_str());
	{
		managed_shared_memory segment(create_only, name.c_str(),
					      8 << 20);
		// 65536 x 1 x 65536 is 2^32: 0 in 32 bits.
		CHECK_THROWS(per_cpu_array_map_impl(segment, 65536, 65536, 1));
	}
	shared_memory_object::remove(name.c_str());
}

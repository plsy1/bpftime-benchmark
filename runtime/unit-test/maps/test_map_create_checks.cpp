// Map creation checks sizes as the kernel does: a size computation that
// wrapped used to allocate a small buffer that accesses then overran (a hash
// map of 2^32 entries of 2 GiB values got 97 MiB, its first lookup SIGSEGV;
// a per-CPU array of 65536 x 65536 bytes got 0), and degenerate sizes were
// accepted (a 0-byte key matched every key).
#include <boost/interprocess/managed_shared_memory.hpp>
#include <bpf_map/userspace/per_cpu_array_map.hpp>
#include <bpftime_shm.hpp>
#include <bpftime_shm_internal.hpp>
#include <bpf_map/array_map_binding.hpp>
#include <handler/handler_manager.hpp>
#include <catch2/catch_test_macros.hpp>
#include <linux/bpf.h>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <dirent.h>
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
		{ "array value over INT_MAX", BPF_MAP_TYPE_ARRAY, 4,
		  0x80000000u, 1, E2BIG },
		{ "array key other than 4 bytes", BPF_MAP_TYPE_ARRAY, 8, 8, 4,
		  EINVAL },
		{ "per-CPU array value over 32 KiB", BPF_MAP_TYPE_PERCPU_ARRAY,
		  4, 32769, 4, ENOMEM },
		{ "per-CPU hash value over 32 KiB", BPF_MAP_TYPE_PERCPU_HASH, 4,
		  32769, 4, ENOMEM },
		{ "hash key and value at 4 MiB - 48", BPF_MAP_TYPE_HASH, 48,
		  (4u << 20) - 96, 1, E2BIG },
		{ "queue value over 4 MiB", BPF_MAP_TYPE_QUEUE, 0,
		  (4u << 20) + 1, 1, E2BIG },
		{ "queue with a key", BPF_MAP_TYPE_QUEUE, 4, 8, 4, EINVAL },
		{ "stack with a key", BPF_MAP_TYPE_STACK, 4, 8, 4, EINVAL },
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
	// Sizes the kernel (6.8) accepts: no key limit of its own for hashes
	// (bf1af650 refused keys over 512 bytes), and a
	// per-CPU hash's key counted once, not per CPU (one with 256-byte keys
	// and 100,000 entries was refused with ENOMEM on a 128 MB segment).
	struct {
		const char *what;
		int type;
		uint32_t key_size, value_size, max_entries;
	} accepted[] = {
		{ "hash key over 512 bytes", BPF_MAP_TYPE_HASH, 513, 8, 4 },
		{ "per-CPU hash with 256-byte keys", BPF_MAP_TYPE_PERCPU_HASH,
		  256, 8, 100000 },
		{ "per-CPU array value of 32 KiB", BPF_MAP_TYPE_PERCPU_ARRAY, 4,
		  32768, 1 },
	};
	for (auto &c : accepted) {
		INFO(c.what);
		errno = 0;
		CHECK(bpftime_maps_create(100, "accepted",
					  { .type = c.type,
					    .key_size = c.key_size,
					    .value_size = c.value_size,
					    .max_ents = c.max_entries }) == 100);
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

// A map whose lower bound fits the segment but whose implementation does not
// (the striped hash allocates about three times the bound): Boost's container
// length_error, not a std::length_error, escaped map_init and left the
// descriptor holding a handler without a map; a later lookup on it crashed
// the tool.
TEST_CASE("A map the segment cannot hold leaves no handler", "[map-create]")
{
	const std::string name =
		"bpftime_map_create_oom_" + std::to_string(getpid());
	shared_memory_object::remove(name.c_str());
	{
		managed_shared_memory segment(create_only, name.c_str(),
					      16 << 20);
		auto *manager = segment.construct<handler_manager>(
			"handler_manager")(segment, MIN_MAX_FD_COUNT);
		(void)map_binding_mutex(segment);
		errno = 0;
		CHECK(manager->set_handler(
			      3,
			      bpf_map_handler(3, "too_big", segment,
					      { .type = BPF_MAP_TYPE_HASH,
						.key_size = 8,
						.value_size = 8,
						.max_ents = 600000 }),
			      segment) == -1);
		CHECK(errno == ENOMEM);
		CHECK_FALSE(manager->is_allocated(3));
		// The descriptor is free for a map that fits.
		CHECK(manager->set_handler(
			      3,
			      bpf_map_handler(3, "fits", segment,
					      { .type = BPF_MAP_TYPE_HASH,
						.key_size = 8,
						.value_size = 8,
						.max_ents = 1000 }),
			      segment) == 3);
		manager->clear_id_at(3, segment);
	}
	shared_memory_object::remove(name.c_str());
}

// Types bpftime does not implement failed with errno 0, which libbpf's
// bpf_map_create turns into fd 0 and bpf_object__create_map takes as
// success. The kernel returns EINVAL for what it does not support here.
TEST_CASE("Unsupported map types fail with EINVAL", "[map-create]")
{
	const std::string name =
		"bpftime-map-create-unsupported-" + std::to_string(getpid());
	const char *had = getenv("BPFTIME_GLOBAL_SHM_NAME");
	const std::string previous = had ? had : "";
	bpftime_destroy_global_shm();
	REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
	bpftime_initialize_global_shm(shm_open_type::SHM_REMOVE_AND_CREATE);
	struct {
		const char *what;
		int type;
		uint32_t key_size;
	} cases[] = {
		{ "sockhash", BPF_MAP_TYPE_SOCKHASH, 4 },
		{ "LRU per-CPU hash", BPF_MAP_TYPE_LRU_PERCPU_HASH, 4 },
		{ "bloom filter with a key", BPF_MAP_TYPE_BLOOM_FILTER, 4 },
	};
	for (auto &c : cases) {
		INFO(c.what);
		errno = 0;
		CHECK(bpftime_maps_create(100, "unsupported",
					  { .type = c.type,
					    .key_size = c.key_size,
					    .value_size = 4,
					    .max_ents = 4 }) == -1);
		CHECK(errno == EINVAL);
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

static int open_descriptors()
{
	int n = 0;
	DIR *dir = opendir("/proc/self/fd");
	while (dir != nullptr && readdir(dir) != nullptr)
		n++;
	if (dir != nullptr)
		closedir(dir);
	return n;
}

// Without a descriptor the shm opens one for the map (the syscall server's
// BPF_MAP_CREATE): a rejected creation kept it open.
TEST_CASE("A rejected map creation closes its descriptor", "[map-create]")
{
	const std::string name =
		"bpftime-map-create-fd-" + std::to_string(getpid());
	const char *had = getenv("BPFTIME_GLOBAL_SHM_NAME");
	const std::string previous = had ? had : "";
	bpftime_destroy_global_shm();
	REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
	bpftime_initialize_global_shm(shm_open_type::SHM_REMOVE_AND_CREATE);
	const int before = open_descriptors();
	for (int i = 0; i < 20; i++) {
		CHECK(bpftime_maps_create(-1, "rejected",
					  { .type = BPF_MAP_TYPE_HASH,
					    .key_size = 0,
					    .value_size = 8,
					    .max_ents = 4 }) == -1);
		CHECK(errno == EINVAL);
	}
	CHECK(open_descriptors() == before);
	shm_holder.global_shared_memory.reset_server_state();
	bpftime_destroy_global_shm();
	shared_memory_object::remove(name.c_str());
	bpftime_initialize_global_shm(shm_open_type::SHM_NO_CREATE);
	if (had)
		setenv("BPFTIME_GLOBAL_SHM_NAME", previous.c_str(), 1);
	else
		unsetenv("BPFTIME_GLOBAL_SHM_NAME");
}

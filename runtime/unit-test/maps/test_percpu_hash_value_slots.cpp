// A program keeps the value pointer of a per-CPU hash lookup after the map
// lock is released. Keys other than 4 bytes kept their values in separately
// allocated vectors that a delete freed at once: the program's next write
// went into memory the segment allocator owned again (its free-block
// links), corrupting every process sharing the segment.
#include <boost/interprocess/managed_shared_memory.hpp>
#include <bpf_map/userspace/per_cpu_hash_map.hpp>
#include <catch2/catch_test_macros.hpp>
#include <linux/bpf.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include "platform_utils.hpp"

using namespace boost::interprocess;
using namespace bpftime;

TEST_CASE("Per-CPU hash values outlive a delete without freeing memory",
	  "[percpu-hash]")
{
	const std::string name =
		"bpftime_percpu_slots_" + std::to_string(getpid());
	shared_memory_object::remove(name.c_str());
	fflush(nullptr);
	// In a child: the failure mode is a corrupted allocator, often a crash.
	pid_t pid = fork();
	if (pid == 0) {
		alarm(60);
		managed_shared_memory segment(create_only, name.c_str(),
					      4 << 20);
		constexpr uint32_t value_size = 64;
		per_cpu_hash_map_impl map(segment, 8, value_size, 64);
		uint64_t key = 1;
		std::vector<uint8_t> value(value_size, 0);
		if (map.elem_update(&key, value.data(), 0) != 0)
			_exit(2);
		auto *p = static_cast<uint8_t *>(map.elem_lookup(&key));
		if (p == nullptr)
			_exit(3);
		if (map.elem_delete(&key) != 0)
			_exit(4);
		// Other users of the segment allocate blocks of the value's size
		// (the freed value, if it was freed, is handed out again) ...
		const size_t block = value_size * bpftime_num_possible_cpus();
		std::vector<uint8_t *> blocks;
		for (int i = 0; i < 64; i++) {
			auto *mem = static_cast<uint8_t *>(
				segment.allocate(block));
			std::memset(mem, 0x11, block);
			blocks.push_back(mem);
		}
		// ... and the program, still running, writes its value.
		std::memset(p, 0xa5, value_size);
		for (auto *mem : blocks)
			for (size_t i = 0; i < block; i++)
				if (mem[i] != 0x11)
					_exit(5);
		if (!segment.check_sanity())
			_exit(8);
		for (auto *mem : blocks)
			segment.deallocate(mem);
		for (uint64_t k = 2; k < 40; k++)
			if (map.elem_update(&k, value.data(), 0) != 0)
				_exit(6);
		_exit(segment.check_sanity() ? 0 : 7);
	}
	int status = 0;
	REQUIRE(waitpid(pid, &status, 0) == pid);
	shared_memory_object::remove(name.c_str());
	INFO("status " << status);
	REQUIRE(WIFEXITED(status));
	REQUIRE(WEXITSTATUS(status) == 0);
}

TEST_CASE("Per-CPU hash update flags mean the same through bpf()",
	  "[percpu-hash]")
{
	// bpftime's own operations use the high 32 bits of the flags; the
	// program path compared the low ones, bpf() the whole value.
	const std::string name =
		"bpftime_percpu_flags_" + std::to_string(getpid());
	shared_memory_object::remove(name.c_str());
	{
		managed_shared_memory segment(create_only, name.c_str(),
					      8 << 20);
		per_cpu_hash_map_impl map(segment, 4, 8, 4);
		std::vector<uint8_t> value(
			8 * (size_t)bpftime_num_possible_cpus(), 1);
		const uint32_t key = 3;
		const uint64_t high = 1ULL << 32;
		REQUIRE(map.elem_update_userspace(&key, value.data(), 0) == 0);
		errno = 0;
		CHECK(map.elem_update_userspace(&key, value.data(),
						high | BPF_NOEXIST) == -1);
		CHECK(errno == EEXIST);
		const uint32_t missing = 4;
		errno = 0;
		CHECK(map.elem_update_userspace(&missing, value.data(),
						high | BPF_EXIST) == -1);
		CHECK(errno == ENOENT);
		CHECK(map.elem_update(&key, value.data(), high | BPF_NOEXIST) ==
		      -1);
	}
	shared_memory_object::remove(name.c_str());
}

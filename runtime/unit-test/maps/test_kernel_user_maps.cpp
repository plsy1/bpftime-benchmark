// Kernel-user proxy maps (daemon mode: programs run in userspace on maps that
// kernel programs share). They need a real kernel map, hence root.
#include "bpf_map/shared/hash_map_kernel_user.hpp"
#include "bpf_map/shared/percpu_array_map_kernel_user.hpp"
#include "catch2/catch_test_macros.hpp"
#include "unit-test/common_def.hpp"
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <cstring>
#include <atomic>
#include <sched.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace bpftime;
using namespace boost::interprocess;

static const char *SHM_NAME = "BPFTIME_TEST_KERNEL_USER_MAPS";

namespace
{
struct value12 {
	uint32_t a, b, c;
};

// The kernel's per-CPU layout: value_size rounded up to 8 bytes per CPU.
constexpr size_t STRIDE = 16;

struct pinned_cpu {
	cpu_set_t old;
	explicit pinned_cpu(int cpu)
	{
		sched_getaffinity(0, sizeof(old), &old);
		cpu_set_t set;
		CPU_ZERO(&set);
		CPU_SET(cpu, &set);
		REQUIRE(sched_setaffinity(0, sizeof(set), &set) == 0);
	}
	~pinned_cpu()
	{
		sched_setaffinity(0, sizeof(old), &old);
	}
};
} // namespace

TEST_CASE("Kernel-user percpu array gives a program its CPU's value",
	  "[maps][kernel_user]")
{
	if (geteuid() != 0)
		SKIP("creating a kernel map needs root");
	const int ncpu = libbpf_num_possible_cpus();
	REQUIRE(ncpu > 0);
	int fd = bpf_map_create(BPF_MAP_TYPE_PERCPU_ARRAY, "ku_percpu", 4,
				sizeof(value12), 2, nullptr);
	REQUIRE(fd >= 0);
	bpf_map_info info = {};
	uint32_t len = sizeof(info);
	REQUIRE(bpf_obj_get_info_by_fd(fd, &info, &len) == 0);

	// CPU c holds {c, 100 + c, 200 + c} at key 1.
	std::vector<uint8_t> all(STRIDE * ncpu);
	for (int c = 0; c < ncpu; c++) {
		value12 v{ (uint32_t)c, 100u + c, 200u + c };
		memcpy(&all[STRIDE * c], &v, sizeof(v));
	}
	uint32_t key = 1;
	REQUIRE(bpf_map_update_elem(fd, &key, all.data(), BPF_ANY) == 0);

	shm_remove remover(SHM_NAME);
	managed_shared_memory mem(create_only, SHM_NAME, 1 << 20);
	percpu_array_map_kernel_user_impl map(mem, info.id);

	const int cpu = ncpu > 1 ? 1 : 0;
	{
		pinned_cpu pin(cpu);
		auto *seen = (const value12 *)map.elem_lookup(&key);
		REQUIRE(seen != nullptr);
		CHECK(seen->a == (uint32_t)cpu);
		CHECK(seen->b == 100u + cpu);
		CHECK(seen->c == 200u + cpu);

		// A program's update changes its CPU's value only.
		value12 mine{ 7, 8, 9 };
		REQUIRE(map.elem_update(&key, &mine, BPF_ANY) == 0);
	}
	std::vector<uint8_t> after(STRIDE * ncpu);
	REQUIRE(bpf_map_lookup_elem(fd, &key, after.data()) == 0);
	for (int c = 0; c < ncpu; c++) {
		value12 v;
		memcpy(&v, &after[STRIDE * c], sizeof(v));
		if (c == cpu) {
			CHECK((v.a == 7 && v.b == 8 && v.c == 9));
		} else {
			CHECK((v.a == (uint32_t)c && v.b == 100u + c &&
			       v.c == 200u + c));
		}
	}

	// Syscall access sees every CPU in the kernel's layout.
	auto *user = (const uint8_t *)map.elem_lookup_userspace(&key);
	REQUIRE(user != nullptr);
	CHECK(memcmp(user, after.data(), after.size()) == 0);
	close(fd);
}

// Go programs run uprobes on many threads at once (DeepFlow's goroutine
// tracking reads proc_info_map from every runtime.execute); each lookup must
// return the value it read, not a buffer another thread is refilling.
TEST_CASE("Kernel-user hash lookups are private to the calling thread",
	  "[maps][kernel_user]")
{
	if (geteuid() != 0)
		SKIP("creating a kernel map needs root");
	constexpr int THREADS = 8;
	int fd = bpf_map_create(BPF_MAP_TYPE_HASH, "ku_hash", 4, 64, THREADS,
				nullptr);
	REQUIRE(fd >= 0);
	bpf_map_info info = {};
	uint32_t len = sizeof(info);
	REQUIRE(bpf_obj_get_info_by_fd(fd, &info, &len) == 0);
	for (uint32_t k = 0; k < THREADS; k++) {
		uint8_t value[64];
		memset(value, (int)k + 1, sizeof(value));
		REQUIRE(bpf_map_update_elem(fd, &k, value, BPF_ANY) == 0);
	}

	shm_remove remover(SHM_NAME);
	managed_shared_memory mem(create_only, SHM_NAME, 1 << 20);
	hash_map_kernel_user_impl map(mem, info.id);

	std::atomic<int> torn{ 0 }, missing{ 0 };
	std::vector<std::thread> threads;
	for (uint32_t t = 0; t < THREADS; t++) {
		threads.emplace_back([&, t] {
			for (int i = 0; i < 20000; i++) {
				auto *v = (const uint8_t *)map.elem_lookup(&t);
				if (v == nullptr) {
					missing++;
					continue;
				}
				for (int b = 0; b < 64; b++) {
					if (v[b] != t + 1) {
						torn++;
						break;
					}
				}
			}
		});
	}
	for (auto &th : threads)
		th.join();
	CHECK(missing.load() == 0);
	CHECK(torn.load() == 0);
	close(fd);
}

// The same R1 load on bpftime's hash map (the header given with -I): two keys
// of one bucket, 2 updater threads writing {key, x, x}, 3 readers looking
// both up with lookup_read; counts misses (a key present throughout not
// found) and copies that are not the key's own whole value.
// Usage: r1bt SECS ROUNDS [CPUS]: CPUS = u0,u1,r0,r1,r2.
#include <bpf_map/bpftime_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <thread>
#include <vector>
using namespace bpftime;
using namespace boost::interprocess;
int bpftime_cpu_index_limit() { return (int)std::thread::hardware_concurrency(); }

static void pin(int cpu)
{
	if (cpu < 0)
		return;
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(cpu, &set);
	if (pthread_setaffinity_np(pthread_self(), sizeof(set), &set)) {
		perror("affinity");
		exit(1);
	}
}

int main(int argc, char **argv)
{
	const int secs = argc > 1 ? atoi(argv[1]) : 2;
	const int rounds = argc > 2 ? atoi(argv[2]) : 5;
	int cpus[5] = { -1, -1, -1, -1, -1 };
	if (argc > 3 && sscanf(argv[3], "%d,%d,%d,%d,%d", &cpus[0], &cpus[1], &cpus[2],
				&cpus[3], &cpus[4]) != 5)
		return 2;
	for (int round = 0; round < rounds; round++) {
		shared_memory_object::remove("bpftime_r1_miss");
		managed_shared_memory seg(create_only, "bpftime_r1_miss", 4 << 20);
		bpftime_hash_map map(seg, 64, 4, 24);
		uint32_t keys[2] = { 1, 0 };
		for (uint32_t k = 2; keys[1] == 0; k++)
			if (map.bucket_of_key(&k) == map.bucket_of_key(&keys[0]))
				keys[1] = k;
		for (const uint32_t &key : keys) {
			const uint64_t init[3] = { key, 0, 0 };
			map.elem_update(&key, init);
		}
		std::atomic<bool> stop{ false };
		std::atomic<uint64_t> reads{ 0 }, misses{ 0 }, torn{ 0 }, updates{ 0 };
		std::vector<std::thread> threads;
		for (int w = 0; w < 2; w++)
			threads.emplace_back([&, w] {
				pin(cpus[w]);
				uint64_t n = 0;
				for (uint64_t x = 1; !stop.load(std::memory_order_relaxed); x++, n++) {
					const uint64_t value[3] = { keys[w], x, x };
					map.elem_update(&keys[w], value);
				}
				updates += n;
			});
		for (int r = 0; r < 3; r++)
			threads.emplace_back([&, r] {
				pin(cpus[2 + r]);
				uint64_t n = 0, miss = 0, bad = 0;
				while (!stop.load(std::memory_order_relaxed))
					for (const uint32_t &key : keys) {
						uint64_t k = 0, a = 0, b = 0;
						const void *p = map.lookup_read(&key, [&](const void *v) {
							auto *q = static_cast<uint64_t *>(const_cast<void *>(v));
							k = std::atomic_ref<uint64_t>(q[0]).load(std::memory_order_acquire);
							a = std::atomic_ref<uint64_t>(q[1]).load(std::memory_order_acquire);
							b = std::atomic_ref<uint64_t>(q[2]).load(std::memory_order_acquire);
						});
						n++;
						if (p == nullptr)
							miss++;
						else if (k != key || a != b)
							bad++;
					}
				reads += n;
				misses += miss;
				torn += bad;
			});
		const auto t0 = std::chrono::steady_clock::now();
		std::this_thread::sleep_for(std::chrono::seconds(secs));
		stop = true;
		for (auto &t : threads)
			t.join();
		const double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		printf("round %d: %.2f s, reads %llu (%.1f M/s), updates %llu (%.1f M/s), misses %llu (%.4f%%), torn %llu\n",
		       round + 1, dt, (unsigned long long)reads.load(), reads / dt / 1e6,
		       (unsigned long long)updates.load(), updates / dt / 1e6,
		       (unsigned long long)misses.load(), reads ? 100.0 * misses / reads : 0.0,
		       (unsigned long long)torn.load());
		fflush(stdout);
		shared_memory_object::remove("bpftime_r1_miss");
	}
	return 0;
}

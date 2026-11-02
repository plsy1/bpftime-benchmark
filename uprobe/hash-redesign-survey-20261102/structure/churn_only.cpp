// Churn at a given load only (for perf): delete a random live key, insert a new one.
#include "bpf_map/bpftime_hash_map.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <unistd.h>
#include <vector>
using namespace boost::interprocess;
int main(int argc, char **argv)
{
	const size_t max_entries = 100000, n = (size_t)(max_entries * atof(argv[1]));
	const size_t ops = atol(argv[2]);
	std::string shm = "bpftime_hash_churn_" + std::to_string(getpid());
	shared_memory_object::remove(shm.c_str());
	{
		managed_shared_memory seg(create_only, shm.c_str(), 64 << 20);
		bpftime::bpftime_hash_map map(seg, max_entries, 8, 8);
		std::vector<uint64_t> live(n);
		uint64_t v = 1, next = 0;
		auto key = [](uint64_t id) { uint64_t x = id * 0x9e3779b97f4a7c15ULL; return x ^ (x >> 29); };
		for (size_t i = 0; i < n; i++) {
			live[i] = next++;
			uint64_t k = key(live[i]);
			map.elem_update(&k, &v);
		}
		std::mt19937_64 rng(1);
		auto t0 = std::chrono::steady_clock::now();
		for (size_t o = 0; o < ops; o++) {
			size_t s = rng() % n;
			uint64_t k = key(live[s]);
			if (!map.elem_delete(&k))
				return 2;
			live[s] = next++;
			k = key(live[s]);
			if (!map.elem_update(&k, &v))
				return 3;
		}
		double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count();
		printf("load %s: %.1f ns per delete+insert, pool %s\n", argv[1], ns / ops, "");
	}
	shared_memory_object::remove(shm.c_str());
	return 0;
}

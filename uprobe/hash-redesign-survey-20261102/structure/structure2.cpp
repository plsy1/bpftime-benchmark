// Structure of the chained bpftime_hash_map under realistic key patterns and
// loads: chain length of stored keys' buckets (disp_*), elements a miss walks
// (miss_probe_*), empty and tomb are 0 (no such state), and single-thread ns per operation, after
// filling to a load and after churn (insert/delete at that load). One JSON
// line per (pattern, key size, load, phase).
#include "bpf_map/bpftime_hash_map.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <unistd.h>
#include <vector>

using namespace boost::interprocess;
using bpftime::bpftime_hash_map;

struct pattern {
	const char *name;
	size_t key_size;
	// key i (distinct for distinct i) into out
	void (*make)(uint64_t i, uint8_t *out, uint64_t salt);
};

static uint64_t mix(uint64_t x)
{
	x ^= x >> 33;
	x *= 0xff51afd7ed558ccdULL;
	x ^= x >> 33;
	return x;
}

static const pattern patterns[] = {
	{ "u32 sequential", 4, [](uint64_t i, uint8_t *o, uint64_t s) { uint32_t k = (uint32_t)(i + s); memcpy(o, &k, 4); } },
	{ "u32 random", 4, [](uint64_t i, uint8_t *o, uint64_t s) { uint32_t k = (uint32_t)(i + s) * 2654435761u; k ^= k >> 16; memcpy(o, &k, 4); } }, // a bijection of u32
	{ "u32 id<<12 (packed field)", 4, [](uint64_t i, uint8_t *o, uint64_t s) { uint32_t k = (uint32_t)((i + s) << 12); memcpy(o, &k, 4); } },
	{ "u32 id<<8", 4, [](uint64_t i, uint8_t *o, uint64_t s) { uint32_t k = (uint32_t)((i + s) << 8); memcpy(o, &k, 4); } },
	{ "u64 sequential", 8, [](uint64_t i, uint8_t *o, uint64_t s) { uint64_t k = i + s; memcpy(o, &k, 8); } },
	{ "u64 heap address (16 B apart)", 8, [](uint64_t i, uint8_t *o, uint64_t s) { uint64_t k = 0xaaaad0000000ULL + (i + s) * 16; memcpy(o, &k, 8); } },
	{ "u64 heap address (4 KiB apart)", 8, [](uint64_t i, uint8_t *o, uint64_t s) { uint64_t k = 0xffff80000000ULL + (i + s) * 4096; memcpy(o, &k, 8); } },
	{ "u64 pid_tgid (tgid 4242, tid i)", 8, [](uint64_t i, uint8_t *o, uint64_t s) { uint64_t k = (4242ULL << 32) | (uint32_t)(i + s); memcpy(o, &k, 8); } },
	{ "u64 random", 8, [](uint64_t i, uint8_t *o, uint64_t s) { uint64_t k = mix(i + s * 1000003); memcpy(o, &k, 8); } },
	{ "16 B {u64 addr, u32 pid, u32 0}", 16, [](uint64_t i, uint8_t *o, uint64_t s) { uint64_t a = 0xaaaad0000000ULL + (i + s) * 64; uint32_t p[2] = { 4242, 0 }; memcpy(o, &a, 8); memcpy(o + 8, p, 8); } },
	{ "12 B {u32 pid, u32 tid, u32 fd}", 12, [](uint64_t i, uint8_t *o, uint64_t s) { uint32_t k[3] = { 4242, (uint32_t)(1000 + (i + s) / 64), (uint32_t)((i + s) % 64) }; memcpy(o, k, 12); } },
	{ "64 B path string", 64, [](uint64_t i, uint8_t *o, uint64_t s) { memset(o, 0, 64); snprintf((char *)o, 64, "/usr/lib/aarch64-linux-gnu/libexample.so.%08llu", (unsigned long long)(i + s)); } },
};

static double now_ns()
{
	return std::chrono::duration<double, std::nano>(
		       std::chrono::steady_clock::now().time_since_epoch())
		.count();
}

int main(int argc, char **argv)
{
	const size_t max_entries = argc > 1 ? atol(argv[1]) : 100000;
	// argv[2]: pattern index (all if absent); argv[3]: churn operations per entry
	const long only = argc > 2 ? atol(argv[2]) : -1;
	const size_t churn_per_entry = argc > 3 ? atol(argv[3]) : 2;
	const size_t value_size = 8;
	std::string shm = "bpftime_hash_structure_" + std::to_string(getpid());
	shared_memory_object::remove(shm.c_str());
	for (size_t pi = 0; pi < sizeof(patterns) / sizeof(patterns[0]); pi++) {
		const auto &p = patterns[pi];
		if (only >= 0 && (size_t)only != pi)
			continue;
		for (double load : { 0.1, 0.5, 0.9, 1.0 }) {
			managed_shared_memory seg(create_only, shm.c_str(),
						  max_entries * (p.key_size + 64) * 3 + (64 << 20));
			{
				bpftime_hash_map map(seg, max_entries, p.key_size, value_size);
				const size_t n = (size_t)(max_entries * load);
				const size_t B = map.bucket_count();
				std::vector<uint8_t> keys(n * p.key_size);
				for (size_t i = 0; i < n; i++)
					p.make(i, &keys[i * p.key_size], 0);
				uint64_t v = 1;
				for (size_t i = 0; i < n; i++)
					if (!map.elem_update(&keys[i * p.key_size], &v))
						return 2;
				double churn_ns = 0;
				auto measure = [&](const char *phase) {
					// chain length of the stored keys' buckets
					uint64_t disp_sum = 0, disp_max = 0;
					for (size_t i = 0; i < n; i++) {
						const uint8_t *k = &keys[i * p.key_size];
						if (!map.elem_lookup(k))
							return false;
						uint64_t d = map.chain_length_of_key(k);
						disp_sum += d;
						disp_max = std::max(disp_max, d);
					}
					// misses: keys of the same pattern not stored; elements a miss walks
					const size_t m = 20000;
					std::vector<uint8_t> miss(m * p.key_size);
					for (size_t i = 0; i < m; i++)
						p.make(i, &miss[i * p.key_size], 1 << 19); // ids above every stored one
					uint64_t probe_sum = 0, probe_max = 0;
					for (size_t i = 0; i < m; i++) {
						uint64_t len = map.chain_length_of_key(&miss[i * p.key_size]);
						probe_sum += len;
						probe_max = std::max(probe_max, len);
					}
					size_t empty = 0, tomb = 0;
					// timing (single thread)
					const size_t reps = std::min<size_t>(n, 200000);
					double t0 = now_ns();
					uint64_t acc = 0;
					for (size_t r = 0; r < reps; r++)
						acc += (uintptr_t)map.elem_lookup(&keys[(r * 7919 % n) * p.key_size]);
					double t1 = now_ns();
					for (size_t r = 0; r < reps; r++)
						acc += (uintptr_t)map.elem_lookup(&miss[(r % m) * p.key_size]);
					double t2 = now_ns();
					printf("{\"pattern\": \"%s\", \"key_size\": %zu, \"load\": %.2f, \"phase\": \"%s\", "
					       "\"buckets\": %zu, \"disp_mean\": %.2f, \"disp_max\": %llu, "
					       "\"miss_probe_mean\": %.2f, \"miss_probe_max\": %llu, "
					       "\"hit_ns\": %.1f, \"miss_ns\": %.1f, \"bytes_per_entry\": %.1f, \"empty_frac\": %.4f, \"tomb_frac\": %.4f, \"churn_ns\": %.1f, \"chk\": %llu}\n",
					       p.name, p.key_size, load, phase, B,
					       n ? (double)disp_sum / n : 0.0, (unsigned long long)disp_max,
					       (double)probe_sum / m, (unsigned long long)probe_max,
					       (t1 - t0) / reps, (t2 - t1) / reps,
					       (double)(seg.get_size() - seg.get_free_memory()) / max_entries,
					       (double)empty / B, (double)tomb / B, churn_ns,
					       (unsigned long long)(acc & 1));
					fflush(stdout);
					return true;
				};
				if (!measure("filled"))
					return 3;
				// churn: delete one, insert a new one, at constant load
				if (n > 0) {
					std::mt19937_64 rng(42);
					const size_t ops = churn_per_entry * max_entries;
					uint64_t next = max_entries; // new ids, distinct from the live ones
					std::vector<uint64_t> ids(n);
					for (size_t i = 0; i < n; i++)
						ids[i] = i;
					std::vector<uint8_t> k(p.key_size);
					double t0 = now_ns();
					for (size_t o = 0; o < ops; o++) {
						size_t slot = rng() % n;
						p.make(ids[slot], k.data(), 0);
						if (!map.elem_delete(k.data()))
							return 4;
						ids[slot] = next++;
						p.make(ids[slot], k.data(), 0);
						if (!map.elem_update(k.data(), &v))
							return 5;
					}
					double t1 = now_ns();
					churn_ns = (t1 - t0) / ops;
					for (size_t i = 0; i < n; i++)
						p.make(ids[i], &keys[i * p.key_size], 0);
					if (!measure("after churn"))
						return 6;
				}
			}
			shared_memory_object::remove(shm.c_str());
		}
	}
	return 0;
}

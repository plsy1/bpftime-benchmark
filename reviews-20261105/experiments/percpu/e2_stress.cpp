// E2: lookups and existing-key updates of keys present throughout, while other
// keys of the same bucket are inserted and deleted (slots and table elements
// reused), from threads using the map through two mappings of the segment at
// different addresses (as two processes would), plus a bpf()-side thread
// copying, iterating and lookup-and-deleting.
// Every key's values carry a key-derived pattern: value for CPU c of key k is
// (k << 32) | (c + 1) on the bpf() side; the program side writes/reads
// (k << 32) | 0x100 | cpu. A wrong slot shows as a pattern of another key.
#include <bpf_map/userspace/per_cpu_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <linux/bpf.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace boost::interprocess;
using namespace bpftime;

static std::atomic<uint64_t> wrong_values{ 0 }, misses{ 0 }, ops{ 0 },
	wrong_copies{ 0 }, update_fail{ 0 };

int main(int argc, char **argv)
{
	const int seconds = argc > 1 ? atoi(argv[1]) : 5;
	const std::string name = "e2_stress_" + std::to_string(getpid());
	shared_memory_object::remove(name.c_str());
	int rc = 0;
	{
		managed_shared_memory seg(create_only, name.c_str(), 32 << 20);
		const int ncpu = bpftime_cpu_index_limit();
		constexpr uint32_t max_entries = 256;
		auto *map = seg.construct<per_cpu_hash_map_impl>(
			anonymous_instance)(seg, 4, 8, max_entries, true);
		auto handle = seg.get_handle_from_address(map);
		// A second mapping of the same segment, at another address.
		managed_shared_memory seg2(open_only, name.c_str());
		auto *map2 = static_cast<per_cpu_hash_map_impl *>(
			seg2.get_address_from_handle(handle));
		printf("mapping 1 at %p, mapping 2 at %p, ncpu %d\n", (void *)map,
		       (void *)map2, ncpu);
		auto &table = map->table_for_test();
		// Keys of one bucket: 8 stable keys plus 64 churn keys.
		std::vector<uint32_t> stable, churn;
		{
			const uint32_t want = table.bucket_of_key(&max_entries);
			for (uint32_t k = 1; stable.size() + churn.size() < 72;
			     k++) {
				if (table.bucket_of_key(&k) != want)
					continue;
				if (stable.size() < 8)
					stable.push_back(k);
				else
					churn.push_back(k);
			}
		}
		printf("bucket keys: %zu stable, %zu churn\n", stable.size(),
		       churn.size());
		const size_t stride = (8 + 7) & ~7u;
		const int ncpu_sys = bpftime_num_possible_cpus();
		auto fill = [&](std::vector<uint8_t> &buf, uint32_t k) {
			buf.assign(stride * ncpu_sys, 0);
			for (int i = 0; i < ncpu_sys; i++) {
				uint64_t v = ((uint64_t)k << 32) |
					     (uint64_t)(bpftime_possible_cpu_ids()[i] + 1);
				memcpy(buf.data() + i * stride, &v, 8);
			}
		};
		std::vector<uint8_t> buf;
		for (uint32_t k : stable) {
			fill(buf, k);
			if (map->elem_update_userspace(&k, buf.data(), BPF_ANY) != 0) {
				perror("fill stable");
				return 2;
			}
		}
		std::atomic<bool> stop{ false };
		std::vector<std::thread> threads;
		// Program-side readers: lookup stable keys, check pattern.
		auto reader = [&](per_cpu_hash_map_impl *m, int id) {
			std::mt19937 gen(id);
			while (!stop.load(std::memory_order_relaxed)) {
				uint32_t k = stable[gen() % stable.size()];
				const int cpu = sched_getcpu();
				auto *p = static_cast<uint64_t *>(m->elem_lookup(&k));
				if (p == nullptr) {
					misses++;
					continue;
				}
				uint64_t v = __atomic_load_n(p, __ATOMIC_RELAXED);
				const uint64_t hi = v >> 32, lo = v & 0xffffffff;
				// Either the bpf()-side pattern or the program-side
				// one, for this key; the CPU may have moved since
				// sched_getcpu, so accept any CPU in the low bits.
				bool ok = hi == k &&
					  ((lo >= 1 && lo <= (uint64_t)ncpu) ||
					   ((lo & 0x100) && (lo & 0xff) < (uint64_t)ncpu));
				if (!ok) {
					wrong_values++;
					if (wrong_values < 5)
						fprintf(stderr,
							"reader %d: key %u cpu %d got %#llx\n",
							id, k, cpu,
							(unsigned long long)v);
				}
				ops++;
			}
		};
		// Program-side updaters of stable keys (existing-key path).
		auto updater = [&](per_cpu_hash_map_impl *m, int id) {
			std::mt19937 gen(100 + id);
			while (!stop.load(std::memory_order_relaxed)) {
				uint32_t k = stable[gen() % stable.size()];
				const int cpu = sched_getcpu();
				const uint64_t v = ((uint64_t)k << 32) | 0x100 |
						   (uint64_t)cpu;
				if (m->elem_update(&k, &v, BPF_EXIST) != 0)
					update_fail++;
				ops++;
			}
		};
		// Churners: insert and delete the churn keys, program and bpf()
		// paths mixed, so that slots and elements are reused.
		auto churner = [&](per_cpu_hash_map_impl *m, int id) {
			std::mt19937 gen(200 + id);
			std::vector<uint8_t> b;
			while (!stop.load(std::memory_order_relaxed)) {
				uint32_t k = churn[gen() % churn.size()];
				switch (gen() % 4) {
				case 0: {
					const uint64_t v = ((uint64_t)k << 32) | 0x100 |
							   (uint64_t)sched_getcpu();
					m->elem_update(&k, &v, BPF_ANY);
					break;
				}
				case 1:
					fill(b, k);
					m->elem_update_userspace(&k, b.data(), BPF_NOEXIST);
					break;
				case 2:
					m->elem_delete(&k);
					break;
				case 3: {
					b.assign(stride * ncpu_sys, 0);
					if (m->lookup_and_delete_userspace(&k, b.data()) == 0) {
						for (int i = 0; i < ncpu_sys; i++) {
							uint64_t v;
							memcpy(&v, b.data() + i * stride, 8);
							if (v != 0 && (v >> 32) != k)
								wrong_copies++;
						}
					}
					break;
				}
				}
				ops++;
			}
		};
		// bpf() side: copies of stable keys and iteration.
		auto tool = [&](per_cpu_hash_map_impl *m) {
			std::vector<uint8_t> b;
			while (!stop.load(std::memory_order_relaxed)) {
				for (uint32_t k : stable) {
					auto *p = static_cast<uint8_t *>(
						m->elem_lookup_userspace(&k));
					if (p == nullptr) {
						misses++;
						continue;
					}
					for (int i = 0; i < ncpu_sys; i++) {
						uint64_t v;
						memcpy(&v, p + i * stride, 8);
						if ((v >> 32) != k) {
							wrong_copies++;
							if (wrong_copies < 5)
								fprintf(stderr,
									"tool: key %u slot %d got %#llx\n",
									k, i,
									(unsigned long long)v);
						}
					}
				}
				uint32_t cur, next;
				int n = 0;
				if (m->map_get_next_key(nullptr, &cur) == 0) {
					n++;
					while (m->map_get_next_key(&cur, &next) == 0 &&
					       n < 100000) {
						cur = next;
						n++;
					}
				}
				if (n < (int)stable.size())
					misses++;
				ops++;
			}
		};
		threads.emplace_back(reader, map, 0);
		threads.emplace_back(reader, map2, 1);
		threads.emplace_back(updater, map2, 0);
		threads.emplace_back(churner, map, 0);
		threads.emplace_back(churner, map2, 1);
		threads.emplace_back(tool, map2);
		std::this_thread::sleep_for(std::chrono::seconds(seconds));
		stop.store(true);
		for (auto &t : threads)
			t.join();
		// Final state: every stable key present with its pattern.
		for (uint32_t k : stable) {
			auto *p = static_cast<uint8_t *>(map2->elem_lookup_userspace(&k));
			if (p == nullptr) {
				misses++;
				continue;
			}
			for (int i = 0; i < ncpu_sys; i++) {
				uint64_t v;
				memcpy(&v, p + i * stride, 8);
				if ((v >> 32) != k)
					wrong_copies++;
			}
		}
		printf("ops %llu, wrong values %llu, wrong copies %llu, misses %llu, update failures %llu\n",
		       (unsigned long long)ops.load(),
		       (unsigned long long)wrong_values.load(),
		       (unsigned long long)wrong_copies.load(),
		       (unsigned long long)misses.load(),
		       (unsigned long long)update_fail.load());
		rc = (wrong_values || wrong_copies || misses || update_fail) ? 1 : 0;
		seg.destroy_ptr(map);
	}
	shared_memory_object::remove(name.c_str());
	return rc;
}

// Capacity accounting: with no concurrent deletes, inserts from many
// threads must succeed exactly max_entries times, wherever the free
// elements sit (spread over the lists, or all on one thread's list after a
// single thread deleted everything, as a tool clearing a map leaves them).
// Then, with concurrent deletes, counts how often an insert fails although
// free elements exist (the documented pcpu_freelist window).
#include "shim.hpp"
#include <bpf_map/bpftime_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

using namespace boost::interprocess;
using namespace bpftime;

int main()
{
	shared_memory_object::remove("hm_capacity");
	managed_shared_memory seg(create_only, "hm_capacity", 64 << 20);
	constexpr uint32_t max_entries = 20000, extra = 1200, threads = 6;
	bpftime_hash_map map(seg, max_entries, 4, 8);
	printf("lists %zu stripes %zu\n", map.free_list_count(), map.stripe_count());
	int rc = 0;
	auto fill = [&](uint32_t base) {
		std::atomic<uint64_t> failed{ 0 };
		std::vector<std::thread> ws;
		for (uint32_t t = 0; t < threads; t++)
			ws.emplace_back([&, t] {
				for (uint32_t i = t; i < max_entries + extra; i += threads) {
					const uint32_t key = base + i;
					const uint64_t v = key;
					if (!map.elem_update(&key, &v))
						failed++;
				}
			});
		for (auto &w : ws)
			w.join();
		return failed.load();
	};
	for (int round = 0; round < 5; round++) {
		const uint32_t base = round * 100000;
		const uint64_t failed = fill(base);
		const size_t count = map.get_elem_count();
		printf("round %d: failed %lu (expect %u) count %zu %s\n", round, failed, extra, count,
		       failed == extra && count == max_entries ? "ok" : "BAD");
		if (failed != extra || count != max_entries)
			rc = 1;
		if (round % 2 == 0) {
			// One thread deletes everything: all free elements on one list.
			for (uint32_t i = 0; i < max_entries + extra; i++) {
				const uint32_t key = base + i;
				map.elem_delete(&key);
			}
		} else {
			std::vector<std::thread> ws;
			for (uint32_t t = 0; t < threads; t++)
				ws.emplace_back([&, t] {
					for (uint32_t i = t; i < max_entries + extra; i += threads) {
						const uint32_t key = base + i;
						map.elem_delete(&key);
					}
				});
			for (auto &w : ws)
				w.join();
		}
		if (map.get_elem_count() != 0) {
			printf("count after clear %zu BAD\n", map.get_elem_count());
			rc = 1;
		}
	}
	// Concurrent deletes: a full map, 3 threads delete and re-insert their
	// own keys; an insert may fail with free elements (the window).
	{
		const uint32_t base = 900000;
		fill(base);
		std::atomic<bool> stop{ false };
		std::atomic<uint64_t> spurious{ 0 }, ops{ 0 };
		std::vector<std::thread> ws;
		for (uint32_t t = 0; t < 3; t++)
			ws.emplace_back([&, t] {
				uint64_t n = 0, sp = 0;
				while (!stop.load(std::memory_order_relaxed)) {
					for (uint32_t i = t; i < 300; i += 3) {
						const uint32_t key = base + i;
						const uint64_t v = key;
						map.elem_delete(&key);
						if (!map.elem_update(&key, &v)) {
							sp++;
							while (!map.elem_update(&key, &v))
								;
						}
						n++;
					}
				}
				ops += n;
				spurious += sp;
			});
		std::this_thread::sleep_for(std::chrono::seconds(3));
		stop = true;
		for (auto &w : ws)
			w.join();
		printf("concurrent deletes: %lu delete+insert, %lu spurious E2BIG, count %zu (expect %u)\n",
		       ops.load(), spurious.load(), map.get_elem_count(), max_entries);
		if (map.get_elem_count() != max_entries)
			rc = 1;
	}
	printf("%s\n", rc == 0 ? "PASS" : "FAIL");
	shared_memory_object::remove("hm_capacity");
	return rc;
}

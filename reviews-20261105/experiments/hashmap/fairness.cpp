// Stripe lock fairness: H "hammer" threads update one key of stripe S in a
// loop (replace path, ~35 ns under the lock each); one "victim" thread does
// single updates of another key of the same stripe, or of another stripe,
// and records the latency of each. The spin lock (exchange 1 / store 0, as
// glibc's generic pthread spin lock and bpftime_spin_lock) has no queue.
// usage: fairness <hammers> <same|other> <seconds>
#include "shim.hpp"
#include <bpf_map/bpftime_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

using namespace boost::interprocess;
using namespace bpftime;
using clk = std::chrono::steady_clock;

int main(int argc, char **argv)
{
	const int hammers = argc > 1 ? atoi(argv[1]) : 2;
	const bool same = argc > 2 && std::strcmp(argv[2], "same") == 0;
	const int seconds = argc > 3 ? atoi(argv[3]) : 2;
	const int gap = argc > 4 ? atoi(argv[4]) : 0; // dummy work iterations between hammer updates
	shared_memory_object::remove("hm_fairness");
	managed_shared_memory seg(create_only, "hm_fairness", 16 << 20);
	bpftime_hash_map map(seg, 4096, 4, 8);
	// Hammer keys: distinct keys of one stripe (distinct buckets if possible).
	std::vector<uint32_t> hk;
	uint32_t victim = 0;
	const uint32_t first = 1;
	const size_t s = map.stripe_of_key(&first);
	for (uint32_t k = 1; hk.size() < (size_t)hammers || victim == 0; k++) {
		const bool in = map.stripe_of_key(&k) == s;
		if (in && hk.size() < (size_t)hammers)
			hk.push_back(k);
		else if (victim == 0 && in == same && k != first)
			victim = k;
	}
	const uint64_t v = 0;
	for (uint32_t k : hk)
		map.elem_update(&k, &v);
	map.elem_update(&victim, &v);
	std::atomic<bool> stop{ false };
	std::atomic<uint64_t> hammer_ops{ 0 };
	std::vector<std::thread> threads;
	for (int h = 0; h < hammers; h++)
		threads.emplace_back([&, h] {
			uint64_t n = 0, x = 0;
			while (!stop.load(std::memory_order_relaxed)) {
				map.elem_update(&hk[h], &x);
				x++;
				n++;
				for (volatile int g = 0; g < gap; g++)
					;
			}
			hammer_ops += n;
		});
	std::vector<uint64_t> lat;
	lat.reserve(1 << 20);
	std::thread vt([&] {
		uint64_t x = 0;
		while (!stop.load(std::memory_order_relaxed)) {
			const auto t0 = clk::now();
			map.elem_update(&victim, &x);
			const auto t1 = clk::now();
			lat.push_back((uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
			x++;
			std::this_thread::sleep_for(std::chrono::microseconds(50));
		}
	});
	std::this_thread::sleep_for(std::chrono::seconds(seconds));
	stop = true;
	for (auto &t : threads)
		t.join();
	vt.join();
	std::sort(lat.begin(), lat.end());
	auto pct = [&](double p) { return lat.empty() ? 0 : lat[std::min(lat.size() - 1, (size_t)(p * lat.size()))]; };
	printf("gap %d hammers %d (%lu updates/s) victim in %s stripe: %zu updates, latency ns p50 %lu p99 %lu p99.9 %lu max %lu\n",
	       gap, hammers, hammer_ops.load() / seconds, same ? "the same" : "another", lat.size(), pct(0.5), pct(0.99),
	       pct(0.999), lat.back());
	shared_memory_object::remove("hm_fairness");
	return 0;
}

// Torn-key false matches for memcmp-path key sizes (not 4): keys K1 and K2
// with the same hash differing at the first two and last two bytes; the
// writer alternates them in one bucket; readers look up the mix, which was
// never inserted. Usage: exp_torn <key_size> <seconds>
#include <bpf_map/bpftime_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
using namespace boost::interprocess;
using namespace bpftime;

int main(int argc, char **argv)
{
	const size_t KS = argc > 1 ? atoi(argv[1]) : 5;
	const int secs = argc > 2 ? atoi(argv[2]) : 3;
	if (KS < 5) {
		printf("need key_size >= 5\n");
		return 1;
	}
	std::string name = "fable2_exp_torn_" + std::to_string(KS);
	shared_memory_object::remove(name.c_str());
	managed_shared_memory seg(create_only, name.c_str(), 16 << 20);
	bpftime_hash_map map(seg, 16, KS, 8);
	auto mk = [&](bool first_b, bool last_b) {
		std::vector<uint8_t> k(KS);
		for (size_t i = 0; i < KS; i++)
			k[i] = (uint8_t)(i * 7 + 3);
		// (1, 0) and (0, 31) contribute the same to the base-31 hash.
		k[0] = first_b ? 0 : 1;
		k[1] = first_b ? 31 : 0;
		k[KS - 2] = last_b ? 0 : 1;
		k[KS - 1] = last_b ? 31 : 0;
		return k;
	};
	auto k1 = mk(false, false), k2 = mk(true, true), mix = mk(false, true),
	     mix2 = mk(true, false);
	if (bpftime_hasher::hash_func(k1.data(), KS) !=
		    bpftime_hasher::hash_func(k2.data(), KS) ||
	    bpftime_hasher::hash_func(k1.data(), KS) !=
		    bpftime_hasher::hash_func(mix.data(), KS)) {
		printf("hash mismatch\n");
		return 1;
	}
	std::atomic<bool> done{ false };
	std::atomic<uint64_t> false_hits{ 0 }, lookups{ 0 }, wrong_val{ 0 };
	std::vector<std::thread> th;
	for (int r = 0; r < 3; r++)
		th.emplace_back([&, r] {
			uint64_t n = 0, bad = 0, wv = 0;
			while (!done.load(std::memory_order_relaxed)) {
				const auto &q = (r & 1) ? mix : mix2;
				if (map.elem_lookup(q.data()))
					bad++;
				// A hit on k1 must carry k1's value (1) or be
				// a bucket deleted meanwhile (value then 2 is
				// acceptable: reuse), never anything else.
				auto *p = (uint64_t *)map.elem_lookup(k1.data());
				if (p) {
					uint64_t v = std::atomic_ref<uint64_t>(*p).load(
						std::memory_order_relaxed);
					if (v != 1 && v != 2)
						wv++;
				}
				n += 2;
			}
			false_hits += bad;
			lookups += n;
			wrong_val += wv;
		});
	const uint64_t v1 = 1, v2 = 2;
	const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(secs);
	uint64_t cycles = 0;
	while (std::chrono::steady_clock::now() < end) {
		for (int i = 0; i < 1000; i++) {
			map.elem_update(k1.data(), &v1);
			map.elem_delete(k1.data());
			map.elem_update(k2.data(), &v2);
			map.elem_delete(k2.data());
		}
		cycles += 1000;
	}
	done = true;
	for (auto &t : th)
		t.join();
	printf("key_size %zu cycles %lu lookups %lu false_hits %lu wrong_val %lu\n",
	       KS, cycles, lookups.load(), false_hits.load(), wrong_val.load());
	shared_memory_object::remove(name.c_str());
	return (false_hits || wrong_val) ? 2 : 0;
}

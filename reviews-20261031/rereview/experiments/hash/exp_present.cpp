// Stress: keys inserted once and never touched must always be found with their
// value; keys never inserted must never be found; while other threads churn
// keys homed in the same stripes (bounds grow/shrink, tombstones released,
// pool drained to E2BIG by a filler). Usage: exp_present <key_size> <seconds>
#include <bpf_map/bpftime_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <unistd.h>
using namespace boost::interprocess;
using namespace bpftime;

static size_t KS;
using hkey = std::vector<uint8_t>;

int main(int argc, char **argv)
{
	KS = argc > 1 ? atoi(argv[1]) : 4;
	const int secs = argc > 2 ? atoi(argv[2]) : 3;
	const char *name = "fable2_exp_present";
	shared_memory_object::remove(name);
	managed_shared_memory seg(create_only, name, 64 << 20);
	constexpr size_t ENTRIES = 2048;
	bpftime_hash_map map(seg, ENTRIES, KS, 8);
	const size_t N = map.bucket_count();
	const size_t S = map.stripe_count(), BPS = map.buckets_per_stripe();
	printf("key_size %zu buckets %zu stripes %zu bps %zu\n", KS, N, S, BPS);

	auto home = [&](const hkey &k) {
		return bpftime_hasher::hash_func(k.data(), KS) % N;
	};
	// Two windows of home buckets: 4 stripes in the middle, and the last
	// stripe + stripe 0 (wrap).
	auto in_window = [&](size_t h) {
		return (h >= 10 * BPS && h < 14 * BPS) ||
		       h >= (S - 1) * BPS || h < BPS;
	};
	std::mt19937_64 rng(12345);
	std::unordered_map<size_t, std::vector<hkey>> by_home;
	std::unordered_set<std::string> seen;
	size_t total = 0;
	for (uint64_t i = 0; total < 40000 && i < 50'000'000; i++) {
		hkey k(KS);
		if (KS == 4) {
			uint32_t v = (uint32_t)i;
			memcpy(k.data(), &v, 4);
		} else {
			for (auto &b : k)
				b = (uint8_t)rng();
		}
		if (!seen.insert(std::string(k.begin(), k.end())).second)
			continue;
		const size_t h = home(k);
		if (!in_window(h))
			continue;
		by_home[h].push_back(k);
		total++;
	}
	std::vector<hkey> stable, never, churn, filler;
	size_t hi = 0;
	for (size_t h = 0; h < N; h++) {
		if (!in_window(h))
			continue;
		auto it = by_home.find(h);
		if (it == by_home.end())
			continue;
		auto &ks = it->second;
		size_t j = 0;
		if (hi % 3 == 0 && j < ks.size())
			stable.push_back(ks[j++]);
		if (hi % 5 == 0 && j < ks.size())
			never.push_back(ks[j++]);
		for (; j < ks.size() && j < 8; j++)
			churn.push_back(ks[j]);
		for (; j < ks.size(); j++)
			filler.push_back(ks[j]);
		hi++;
	}
	printf("stable %zu never %zu churn %zu filler %zu\n", stable.size(),
	       never.size(), churn.size(), filler.size());
	auto fval = [&](const hkey &k) {
		uint64_t v = 0x9e3779b97f4a7c15ULL;
		for (auto b : k)
			v = v * 31 + b;
		return v;
	};
	for (auto &k : stable) {
		uint64_t v = fval(k);
		if (!map.elem_update(k.data(), &v)) {
			printf("stable insert failed\n");
			return 1;
		}
	}
	std::atomic<bool> done{ false };
	std::atomic<uint64_t> misses{ 0 }, wrong{ 0 }, false_hits{ 0 },
		lookups{ 0 }, churn_errors{ 0 }, e2big{ 0 }, max_bound{ 0 };
	std::vector<std::thread> th;
	const int readers = 3, churners = 2;
	for (int r = 0; r < readers; r++)
		th.emplace_back([&, r] {
			uint64_t n = 0, m = 0, w = 0, f = 0;
			size_t i = r;
			while (!done.load(std::memory_order_relaxed)) {
				const auto &k = stable[i % stable.size()];
				auto *p = static_cast<uint64_t *>(
					map.elem_lookup(k.data()));
				if (!p)
					m++;
				else if (*p != fval(k))
					w++;
				const auto &nk = never[i % never.size()];
				if (map.elem_lookup(nk.data()))
					f++;
				n += 2;
				i++;
			}
			lookups += n;
			misses += m;
			wrong += w;
			false_hits += f;
		});
	for (int c = 0; c < churners; c++)
		th.emplace_back([&, c] {
			std::mt19937_64 r(c + 7);
			std::vector<hkey> mine;
			for (size_t i = c; i < churn.size(); i += churners)
				mine.push_back(churn[i]);
			std::vector<uint8_t> present(mine.size(), 0);
			std::vector<uint64_t> val(mine.size(), 0);
			uint64_t err = 0;
			while (!done.load(std::memory_order_relaxed)) {
				const size_t i = r() % mine.size();
				const auto &k = mine[i];
				switch (r() % 3) {
				case 0: {
					uint64_t v = r();
					if (map.elem_update(k.data(), &v)) {
						present[i] = 1;
						val[i] = v;
					} else
						e2big++;
					break;
				}
				case 1: {
					bool ok = map.elem_delete(k.data());
					if (ok != (bool)present[i])
						err++;
					present[i] = 0;
					break;
				}
				default: {
					auto *p = static_cast<uint64_t *>(
						map.elem_lookup(k.data()));
					if ((p != nullptr) != (bool)present[i])
						err++;
					else if (p && *p != val[i])
						err++;
				}
				}
			}
			churn_errors += err;
		});
	// Filler: drives the map to E2BIG and back (token pool drained,
	// take_token_any, far displacements).
	th.emplace_back([&] {
		std::vector<hkey> ins;
		while (!done.load(std::memory_order_relaxed)) {
			ins.clear();
			for (auto &k : filler) {
				uint64_t v = 1;
				if (!map.elem_update(k.data(), &v)) {
					e2big++;
					break;
				}
				ins.push_back(k);
			}
			size_t b = map.max_displacement();
			uint64_t cur = max_bound.load();
			while (b > cur && !max_bound.compare_exchange_weak(cur, b))
				;
			for (auto &k : ins)
				if (!map.elem_delete(k.data()))
					churn_errors++;
		}
	});
	std::this_thread::sleep_for(std::chrono::seconds(secs));
	done = true;
	for (auto &t : th)
		t.join();
	// Final: every stable key present with its value, no never key present.
	size_t final_bad = 0;
	for (auto &k : stable) {
		auto *p = static_cast<uint64_t *>(map.elem_lookup(k.data()));
		if (!p || *p != fval(k))
			final_bad++;
	}
	for (auto &k : never)
		if (map.elem_lookup(k.data()))
			final_bad++;
	printf("lookups %lu misses %lu wrong %lu false_hits %lu churn_errors %lu "
	       "e2big %lu max_bound %lu final_bad %zu count %zu\n",
	       lookups.load(), misses.load(), wrong.load(), false_hits.load(),
	       churn_errors.load(), e2big.load(), max_bound.load(), final_bad,
	       map.get_elem_count());
	shared_memory_object::remove(name);
	return (misses || wrong || false_hits || churn_errors || final_bad) ? 2 :
									      0;
}

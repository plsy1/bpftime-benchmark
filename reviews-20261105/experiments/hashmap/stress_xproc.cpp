// Cross-process stress of bpftime_hash_map at 94f9ba8f.
// Parent = "traced process": updater threads replace present keys, churn
// threads insert/update/delete their own keys against private references,
// reader threads look present keys up without a lock.
// Child = "tool": get_next_key iteration, value copies, its own inserts and
// lookup_and_delete, all through the same shared segment mapped elsewhere.
// Invariants: lookups of keys present throughout never miss; copies made by
// lookup_read are whole and belong to the key; every churn result agrees with
// the thread's reference; iteration terminates and sees every present key;
// the final contents equal the references and get_elem_count is exact.
// usage: stress_xproc <key_size 4|8|16> <spread|concentrated> <seconds>
#include "shim.hpp"
#include <bpf_map/bpftime_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <set>
#include <thread>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

using namespace boost::interprocess;
using namespace bpftime;
using clk = std::chrono::steady_clock;

static const char *SEG = "hm_stress_xproc";
struct value_t {
	uint64_t key, a, b;
};
struct shared_state {
	std::atomic<int> stop{ 0 };
	std::atomic<uint64_t> tool_misses{ 0 }, tool_torn{ 0 }, tool_lookups{ 0 };
	std::atomic<uint64_t> tool_passes{ 0 }, tool_unfinished{ 0 },
		tool_present_missing{ 0 }, tool_bad{ 0 }, tool_lad{ 0 };
	std::atomic<uint64_t> tool_steps_max{ 0 };
};

static size_t KS = 4;
static void make_key(uint64_t n, uint8_t *out)
{
	std::memset(out, 0, 16);
	std::memcpy(out, &n, KS >= 8 ? 8 : 4);
	if (KS == 16) {
		uint64_t m = ~n * 0x9e3779b97f4a7c15ULL;
		std::memcpy(out + 8, &m, 8);
	}
}
static uint64_t key_num(const uint8_t *k)
{
	uint64_t n = 0;
	std::memcpy(&n, k, KS >= 8 ? 8 : 4);
	return n;
}

int main(int argc, char **argv)
{
	KS = argc > 1 ? atoi(argv[1]) : 4;
	const bool concentrated = argc > 2 && std::strcmp(argv[2], "concentrated") == 0;
	const int seconds = argc > 3 ? atoi(argv[3]) : 5;
	shared_memory_object::remove(SEG);
	managed_shared_memory seg(create_only, SEG, 64 << 20);
	constexpr size_t max_entries = 4096;
	auto *map = seg.construct<bpftime_hash_map>("map")(seg, max_entries, KS, sizeof(value_t));
	auto *st = seg.construct<shared_state>("state")();

	// Keys: numbers n; in concentrated mode, only those of 4 chosen buckets
	// (long chains, every writer rearranges chains readers walk).
	const size_t nb = map->bucket_count();
	std::set<uint32_t> chosen;
	auto accept = [&](uint64_t n) {
		uint8_t k[16];
		make_key(n, k);
		const uint32_t b = map->bucket_of_key(k);
		if (!concentrated)
			return true;
		if (chosen.size() < 4)
			chosen.insert(b);
		return chosen.count(b) != 0;
	};
	std::vector<uint64_t> present, churn[3], elsewhere, tool_keys;
	uint64_t n = 1;
	auto take = [&](std::vector<uint64_t> &into, size_t count, bool same) {
		while (into.size() < count) {
			uint8_t k[16];
			make_key(n, k);
			const bool ok = same ? accept(n) : !concentrated || chosen.count(map->bucket_of_key(k)) == 0;
			if (ok)
				into.push_back(n);
			n++;
		}
	};
	take(present, concentrated ? 8 : 64, true);
	for (auto &c : churn)
		take(c, concentrated ? 60 : 400, true);
	take(tool_keys, concentrated ? 40 : 200, true);
	take(elsewhere, 200, false);
	(void)nb;
	for (uint64_t p : present) {
		uint8_t k[16];
		make_key(p, k);
		value_t v{ p, 0, 0 };
		if (!map->elem_update(k, &v)) {
			printf("FAIL initial insert\n");
			return 1;
		}
	}
	const pid_t child = fork();
	if (child == 0) {
		managed_shared_memory s2(open_only, SEG);
		auto *m = s2.find<bpftime_hash_map>("map").first;
		auto *sh = s2.find<shared_state>("state").first;
		std::vector<uint8_t> keybuf(16), next(16);
		uint64_t steps_max = 0;
		while (!sh->stop.load()) {
			// 1. iteration
			size_t steps = 0, seen_present = 0;
			std::set<uint64_t> seen;
			const void *prev = nullptr;
			bool ok;
			while ((ok = m->next_key(prev, next.data()))) {
				steps++;
				const uint64_t kn = key_num(next.data());
				if (seen.insert(kn).second &&
				    std::find(present.begin(), present.end(), kn) != present.end())
					seen_present++;
				keybuf = next;
				prev = keybuf.data();
				if (steps > 2000000)
					break;
			}
			if (ok)
				sh->tool_unfinished++;
			else if (seen_present != present.size())
				sh->tool_present_missing++;
			sh->tool_passes++;
			steps_max = std::max(steps_max, (uint64_t)steps);
			// 2. value copies of present keys
			for (uint64_t p : present) {
				uint8_t k[16];
				make_key(p, k);
				uint64_t kk = 0, a = 0, b = 0;
				const void *r = m->lookup_read(k, [&](const void *v) {
					auto *q = static_cast<uint64_t *>(const_cast<void *>(v));
					kk = std::atomic_ref<uint64_t>(q[0]).load(std::memory_order_acquire);
					a = std::atomic_ref<uint64_t>(q[1]).load(std::memory_order_acquire);
					b = std::atomic_ref<uint64_t>(q[2]).load(std::memory_order_acquire);
				});
				sh->tool_lookups++;
				if (r == nullptr)
					sh->tool_misses++;
				else if (kk != p || a != b)
					sh->tool_torn++;
			}
			// 3. own keys: insert, then lookup_and_delete with the value
			for (uint64_t t : tool_keys) {
				uint8_t k[16];
				make_key(t, k);
				value_t v{ t, 77, 77 };
				if (m->update(k, &v, bpftime_hash_map::update_mode::no_exist) !=
				    bpftime_hash_map::update_result::ok)
					sh->tool_bad++;
			}
			for (uint64_t t : tool_keys) {
				uint8_t k[16];
				make_key(t, k);
				value_t v{};
				if (!m->elem_delete(k, &v))
					sh->tool_bad++;
				else if (v.key != t || v.a != 77 || v.b != 77)
					sh->tool_bad++;
				else
					sh->tool_lad++;
			}
		}
		sh->tool_steps_max = steps_max;
		_exit(0);
	}
	std::atomic<bool> stop{ false };
	std::atomic<uint64_t> misses{ 0 }, torn{ 0 }, lookups{ 0 }, churn_bad{ 0 }, updates{ 0 }, churn_ops{ 0 };
	std::vector<std::thread> threads;
	std::vector<std::map<uint64_t, uint64_t>> refs(3);
	for (int w = 0; w < 2; w++)
		threads.emplace_back([&, w] {
			uint64_t x = 1, nupd = 0;
			while (!stop.load(std::memory_order_relaxed)) {
				for (size_t i = w; i < present.size(); i += 2) {
					uint8_t k[16];
					make_key(present[i], k);
					value_t v{ present[i], x, x };
					if (!map->elem_update(k, &v))
						churn_bad++;
					nupd++;
				}
				x++;
			}
			updates += nupd;
		});
	for (int c = 0; c < 3; c++)
		threads.emplace_back([&, c] {
			std::mt19937_64 rng(99 + c);
			auto &ref = refs[c];
			uint64_t bad = 0, ops = 0;
			while (!stop.load(std::memory_order_relaxed)) {
				const bool use_else = rng() % 8 == 0;
				const auto &pool = use_else ? elsewhere : churn[c];
				const uint64_t kn = use_else ? pool[(rng() % (pool.size() / 3)) + c * (pool.size() / 3)] : pool[rng() % pool.size()];
				uint8_t k[16];
				make_key(kn, k);
				const auto it = ref.find(kn);
				const uint64_t stamp = ++ops;
				value_t v{ kn, stamp, stamp };
				switch (rng() % 4) {
				case 0: {
					const auto r = map->update(k, &v, bpftime_hash_map::update_mode::no_exist);
					if (it == ref.end()) {
						if (r == bpftime_hash_map::update_result::ok)
							ref[kn] = stamp;
						else if (r != bpftime_hash_map::update_result::full)
							bad++;
					} else if (r != bpftime_hash_map::update_result::exists)
						bad++;
					break;
				}
				case 1: {
					const auto r = map->update(k, &v, bpftime_hash_map::update_mode::exist);
					if (it == ref.end()) {
						if (r != bpftime_hash_map::update_result::missing)
							bad++;
					} else if (r != bpftime_hash_map::update_result::ok)
						bad++;
					else
						it->second = stamp;
					break;
				}
				case 2: {
					const bool r = map->elem_delete(k);
					if (r != (it != ref.end()))
						bad++;
					if (it != ref.end())
						ref.erase(it);
					break;
				}
				default: {
					uint64_t kk = 0, a = 0, b = 0;
					const void *r = map->lookup_read(k, [&](const void *vv) {
						auto *q = static_cast<uint64_t *>(const_cast<void *>(vv));
						kk = std::atomic_ref<uint64_t>(q[0]).load(std::memory_order_acquire);
						a = std::atomic_ref<uint64_t>(q[1]).load(std::memory_order_acquire);
						b = std::atomic_ref<uint64_t>(q[2]).load(std::memory_order_acquire);
					});
					if (it == ref.end()) {
						if (r != nullptr)
							bad++;
					} else if (r == nullptr || kk != kn || a != it->second || b != it->second)
						bad++;
				}
				}
			}
			churn_bad += bad;
			churn_ops += ops;
		});
	for (int r = 0; r < 2; r++)
		threads.emplace_back([&] {
			uint64_t miss = 0, bad = 0, nn = 0;
			while (!stop.load(std::memory_order_relaxed)) {
				for (uint64_t p : present) {
					uint8_t k[16];
					make_key(p, k);
					uint64_t kk = 0, a = 0, b = 0;
					const void *r = map->lookup_read(k, [&](const void *v) {
						auto *q = static_cast<uint64_t *>(const_cast<void *>(v));
						kk = std::atomic_ref<uint64_t>(q[0]).load(std::memory_order_acquire);
						a = std::atomic_ref<uint64_t>(q[1]).load(std::memory_order_acquire);
						b = std::atomic_ref<uint64_t>(q[2]).load(std::memory_order_acquire);
					});
					nn++;
					if (r == nullptr)
						miss++;
					else if (kk != p || a != b)
						bad++;
					if (map->elem_lookup(k) == nullptr)
						miss++;
					nn++;
				}
			}
			misses += miss;
			torn += bad;
			lookups += nn;
		});
	std::this_thread::sleep_for(std::chrono::seconds(seconds));
	stop = true;
	st->stop = 1;
	for (auto &t : threads)
		t.join();
	int status = 0;
	waitpid(child, &status, 0);
	// Final state.
	size_t expected = present.size();
	uint64_t final_bad = 0;
	for (auto &ref : refs) {
		expected += ref.size();
		for (auto &[kn, stamp] : ref) {
			uint8_t k[16];
			make_key(kn, k);
			auto *v = static_cast<const value_t *>(map->elem_lookup(k));
			if (v == nullptr || v->key != kn || v->a != stamp || v->b != stamp)
				final_bad++;
		}
	}
	std::set<uint64_t> seen;
	std::vector<uint8_t> keybuf(16), next(16);
	const void *prev = nullptr;
	while (map->next_key(prev, next.data())) {
		seen.insert(key_num(next.data()));
		keybuf = next;
		prev = keybuf.data();
	}
	printf("key_size %zu %s %ds: traced lookups %lu misses %lu torn %lu; updates %lu churn ops %lu bad %lu\n",
	       KS, concentrated ? "concentrated" : "spread", seconds, lookups.load(), misses.load(), torn.load(),
	       updates.load(), churn_ops.load(), churn_bad.load());
	printf("tool: passes %lu unfinished %lu present_missing %lu steps_max %lu lookups %lu misses %lu torn %lu lad %lu bad %lu exit %d\n",
	       st->tool_passes.load(), st->tool_unfinished.load(), st->tool_present_missing.load(),
	       st->tool_steps_max.load(), st->tool_lookups.load(), st->tool_misses.load(), st->tool_torn.load(),
	       st->tool_lad.load(), st->tool_bad.load(), status);
	printf("final: expected %zu iterated %zu count %zu final_bad %lu\n", expected, seen.size(),
	       map->get_elem_count(), final_bad);
	const bool ok = misses == 0 && torn == 0 && churn_bad == 0 && st->tool_misses == 0 && st->tool_torn == 0 &&
			st->tool_present_missing == 0 && st->tool_bad == 0 && seen.size() == expected &&
			map->get_elem_count() == expected && final_bad == 0 && status == 0;
	printf("%s\n", ok ? "PASS" : "FAIL");
	shared_memory_object::remove(SEG);
	return ok ? 0 : 1;
}

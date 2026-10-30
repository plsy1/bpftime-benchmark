// Adversarial stress test for bpftime_hash_map (standalone).
// Usage: stress <max_entries> <key_size:4|16> <owners> <probers> <seconds> [seed]
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
#include <unistd.h>

using namespace boost::interprocess;
using namespace bpftime;

struct value_t {
	uint64_t version;
	uint8_t key[16];
	uint64_t version2;
};

static size_t KEY_SIZE;

struct skey {
	uint8_t b[16];
	bool operator<(const skey &o) const { return memcmp(b, o.b, 16) < 0; }
	bool operator==(const skey &o) const { return memcmp(b, o.b, 16) == 0; }
};

// Build a group of 4 keys with identical hash: variants (A|B) x (X|Y).
// For 4-byte keys: b2,b3 pair (1,0) vs (0,31) -> same hash contribution.
// b0,b1 encode the group id (clustered: small values so the homes are dense).
// For 16-byte keys: positions (6,7) and (14,15) carry the pair trick.
static skey make_key(uint32_t group, bool B, bool Y)
{
	skey k{};
	if (KEY_SIZE == 4) {
		// hash = b0*29791 + b1*961 + b2*31 + b3
		// group spread over b1 and b0 so that homes are spread
		// densely across the table around a few clusters.
		k.b[0] = (uint8_t)(group / 7 % 256);
		k.b[1] = (uint8_t)(group % 7);
		// (1,0) and (0,31) give the same contribution 31.
		// To get two independent variants we need pairs that differ:
		// A: b2=1,b3=0 ; B: b2=0,b3=31   (same hash)
		// Y: we cannot add a second independent pair with 4 bytes, so
		// use B only; X/Y collapse onto the same key for 4-byte keys.
		if (B) { k.b[2] = 0; k.b[3] = 31; } else { k.b[2] = 1; k.b[3] = 0; }
		(void)Y;
	} else {
		k.b[0] = (uint8_t)(group & 0xff);
		k.b[1] = (uint8_t)((group >> 8) & 0xff);
		k.b[2] = (uint8_t)((group >> 16) & 0xff);
		k.b[3] = 7;
		if (B) { k.b[6] = 0; k.b[7] = 31; } else { k.b[6] = 1; k.b[7] = 0; }
		if (Y) { k.b[14] = 0; k.b[15] = 31; } else { k.b[14] = 1; k.b[15] = 0; }
	}
	return k;
}

int main(int argc, char **argv)
{
	if (argc < 6) {
		fprintf(stderr, "usage\n");
		return 2;
	}
	const size_t max_entries = strtoul(argv[1], nullptr, 0);
	KEY_SIZE = strtoul(argv[2], nullptr, 0);
	const int owners = atoi(argv[3]);
	const int probers = atoi(argv[4]);
	const double seconds = atof(argv[5]);
	const unsigned seed = argc > 6 ? atoi(argv[6]) : 1;
	const unsigned insert_pct = argc > 7 ? atoi(argv[7]) : 25;
	const unsigned delete_pct = argc > 8 ? atoi(argv[8]) : 25;
	std::atomic<int64_t> present_total{0};
	std::atomic<uint64_t> full_results{0}, bad_full{0};
	const char *name = "FABLE_STRESS_HASH";
	shared_memory_object::remove(name);
	managed_shared_memory segment(create_only, name, 256 << 20);
	bpftime_hash_map map(segment, max_entries, KEY_SIZE, sizeof(value_t));
	printf("buckets=%zu stripes=%zu per_stripe=%zu\n", map.bucket_count(),
	       map.stripe_count(), map.buckets_per_stripe());

	// Groups: enough keys that the map can be at capacity.
	// Each owner owns groups g where g % owners == owner.
	const uint32_t groups = (uint32_t)(max_entries * 3 / 4); // 2 keys per group (A,B) inserted
	// per-key delete counters for value checks by probers (index = group*2+B)
	std::vector<std::atomic<uint64_t>> started(groups * 2), finished(groups * 2);
	for (auto &a : started) a = 0;
	for (auto &a : finished) a = 0;

	std::atomic<bool> done{ false };
	std::atomic<uint64_t> errors{ 0 }, false_hits{ 0 }, wrong_values{ 0 },
		prober_lookups{ 0 }, owner_ops{ 0 };
	std::vector<std::atomic<uint64_t>> progress(owners + probers);
	for (auto &p : progress) p = 0;
	std::vector<std::map<skey, uint64_t>> refs(owners);
	std::vector<std::thread> threads;

	for (int t = 0; t < owners; t++)
		threads.emplace_back([&, t] {
			std::mt19937 rng(seed * 1000 + t);
			auto &ref = refs[t];
			uint64_t bad = 0, ops = 0;
			char what[256];
			while (!done.load(std::memory_order_relaxed)) {
				const uint32_t g = (uint32_t)(rng() % groups);
				if (g % owners != (uint32_t)t) continue;
				const bool B = rng() & 1;
				const skey key = make_key(g, B, false); // X variant inserted (A,X),(B,X)
				const size_t idx = g * 2 + B;
				const uint64_t version = ((uint64_t)t << 48) | ops;
				value_t val{};
				val.version = version;
				memcpy(val.key, key.b, 16);
				val.version2 = version;
				auto it = ref.find(key);
				const int op = [&]{ unsigned r = rng() % 100; return r < insert_pct ? 0 : r < insert_pct + 10 ? 1 : r < insert_pct + 10 + delete_pct ? 2 : 3; }();
				ops++;
				switch (op) {
				case 0: { // insert NOEXIST
					auto r = map.update(key.b, &val, bpftime_hash_map::update_mode::no_exist);
					if (it == ref.end()) {
						if (r != bpftime_hash_map::update_result::ok) {
							// full is only acceptable if map is truly full; we cannot know
							// exactly here, so record it separately
							if (r == bpftime_hash_map::update_result::full) {
								full_results++;
								if (present_total.load() + owners < (int64_t)max_entries) bad_full++;
							} else { bad++; snprintf(what, sizeof what, "insert new got %d", (int)r); fprintf(stderr, "%s\n", what); }
						} else {
							ref[key] = version; present_total++; }
					} else if (r != bpftime_hash_map::update_result::exists) {
						bad++; fprintf(stderr, "insert existing got %d\n", (int)r);
					}
					break;
				}
				case 1: { // update EXIST
					auto r = map.update(key.b, &val, bpftime_hash_map::update_mode::exist);
					if (it == ref.end()) {
						if (r != bpftime_hash_map::update_result::missing) { bad++; fprintf(stderr, "update missing got %d\n", (int)r); }
					} else {
						if (r != bpftime_hash_map::update_result::ok) { bad++; fprintf(stderr, "update existing got %d\n", (int)r); }
						else it->second = version;
					}
					break;
				}
				case 2: { // delete
					started[idx].fetch_add(1);
					const bool r = map.elem_delete(key.b);
					finished[idx].fetch_add(1);
					if (it == ref.end()) {
						if (r) { bad++; fprintf(stderr, "delete missing succeeded\n"); }
					} else {
						if (!r) { bad++; fprintf(stderr, "delete existing failed\n"); }
						ref.erase(it); present_total--;
					}
					break;
				}
				default: { // lookup
					auto *v = static_cast<value_t *>(map.elem_lookup(key.b));
					if (it == ref.end()) {
						if (v) { bad++; fprintf(stderr, "owner lookup absent key found\n"); }
					} else {
						if (!v) { bad++; fprintf(stderr, "owner lookup present key missing (FALSE NEGATIVE)\n"); }
						else {
							value_t copy;
							memcpy(&copy, v, sizeof copy);
							if (copy.version != it->second || copy.version2 != it->second ||
							    memcmp(copy.key, key.b, 16) != 0) {
								bad++; fprintf(stderr, "owner lookup wrong value\n");
							}
						}
					}
				}
				}
				progress[t].fetch_add(1, std::memory_order_relaxed);
			}
			errors += bad;
			owner_ops += ops;
		});

	for (int p = 0; p < probers; p++)
		threads.emplace_back([&, p] {
			std::mt19937 rng(seed * 7919 + p);
			uint64_t fh = 0, wv = 0, n = 0;
			while (!done.load(std::memory_order_relaxed)) {
				const uint32_t g = (uint32_t)(rng() % groups);
				const bool B = rng() & 1;
				if (KEY_SIZE != 4 && (rng() & 1)) {
					// never-inserted Y variant with same hash: must never hit
					const skey k = make_key(g, B, true);
					if (map.elem_lookup(k.b) != nullptr) fh++;
				} else {
					const skey k = make_key(g, B, false);
					const size_t idx = g * 2 + B;
					const uint64_t before = finished[idx].load();
					auto *v = static_cast<value_t *>(map.elem_lookup(k.b));
					if (v) {
						uint8_t kb[16];
						memcpy(kb, v->key, 16);
						std::atomic_thread_fence(std::memory_order_seq_cst);
						if (started[idx].load() == before &&
						    memcmp(kb, k.b, 16) != 0)
							wv++;
					}
				}
				n++;
				progress[owners + p].fetch_add(1, std::memory_order_relaxed);
			}
			false_hits += fh;
			wrong_values += wv;
			prober_lookups += n;
		});

	// Watchdog
	const auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
	std::vector<uint64_t> last(owners + probers, 0);
	int stuck_rounds = 0;
	while (std::chrono::steady_clock::now() < end) {
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		bool stuck = false;
		for (size_t i = 0; i < last.size(); i++) {
			uint64_t now = progress[i].load();
			if (now == last[i]) stuck = true;
			last[i] = now;
		}
		if (stuck) {
			if (++stuck_rounds >= 10) {
				fprintf(stderr, "HANG: a thread made no progress for 5 s\n");
				fflush(stderr);
				_exit(99);
			}
		} else stuck_rounds = 0;
	}
	done = true;
	for (auto &t : threads) t.join();

	// Final consistency
	size_t present = 0;
	std::set<skey> all;
	for (auto &r : refs) { present += r.size(); for (auto &kv : r) all.insert(kv.first); }
	uint64_t bad = 0;
	for (auto &r : refs)
		for (auto &kv : r) {
			auto *v = static_cast<value_t *>(map.elem_lookup(kv.first.b));
			if (!v || v->version != kv.second) bad++;
		}
	const size_t count = map.get_elem_count();
	// next_key enumeration
	std::set<skey> seen;
	skey cur{}, next{};
	const void *prev = nullptr;
	while (map.next_key(prev, next.b)) {
		if (!seen.insert(next).second) { bad++; fprintf(stderr, "next_key duplicate\n"); break; }
		cur = next; prev = cur.b;
	}
	// fill to capacity
	size_t filled = 0;
	for (uint32_t i = 0;; i++) {
		skey k{}; k.b[0] = 0xff; k.b[1] = 0xff; memcpy(k.b + 2, &i, 4); if (KEY_SIZE==4) { k.b[0]=(uint8_t)(i>>16)|0x80; k.b[1]=(uint8_t)(i>>8); k.b[2]=(uint8_t)i; k.b[3]=0xff; }
		value_t val{};
		auto r = map.update(k.b, &val, bpftime_hash_map::update_mode::no_exist);
		if (r == bpftime_hash_map::update_result::ok) { filled++; continue; }
		if (r != bpftime_hash_map::update_result::full) { bad++; fprintf(stderr, "fill got %d\n", (int)r); }
		break;
	}
	printf("owner_ops=%lu prober_lookups=%lu errors=%lu false_hits=%lu wrong_values=%lu full_results=%lu bad_full=%lu\n",
	       owner_ops.load(), prober_lookups.load(), errors.load(), false_hits.load(), wrong_values.load(), full_results.load(), bad_full.load());
	printf("present=%zu elem_count=%zu enumerated=%zu filled=%zu (expected %zu) max_disp=%zu final_bad=%lu\n",
	       present, count, seen.size(), filled, max_entries - present, map.max_displacement(), bad);
	bool ok = errors == 0 && false_hits == 0 && wrong_values == 0 && bad == 0 &&
		  count == present && seen.size() == present && filled == max_entries - present && seen == all;
	printf("%s\n", ok ? "PASS" : "FAIL");
	shared_memory_object::remove(name);
	return ok ? 0 : 1;
}

// Reproducers for the review of hash map step 2 (REVIEW.md, findings 1, 2, 3,
// 5, 6), against the table header of dev e5ba4aa7. One case per argument.
#include "bpf_map/bpftime_hash_map.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace boost::interprocess;
using bpftime::bpftime_hash_map;
using clk = std::chrono::steady_clock;
using mode = bpftime_hash_map::update_mode;
using result = bpftime_hash_map::update_result;

// The table sizes its free lists from this (platform_utils.cpp otherwise).
int bpftime_cpu_index_limit()
{
	return 6;
}

static const char *SHM = "bpftime_review_repro";

struct segment {
	managed_shared_memory mem;
	explicit segment(size_t bytes)
		: mem((shared_memory_object::remove(SHM), open_or_create), SHM,
		      bytes)
	{
	}
	~segment()
	{
		shared_memory_object::remove(SHM);
	}
};

// Runs fn in a thread; true when it returned within ms.
template <class Fn> static bool returns_within(int ms, Fn fn, std::thread &t,
					       std::atomic<bool> &done)
{
	t = std::thread([&] {
		fn();
		done = true;
	});
	auto deadline = clk::now() + std::chrono::milliseconds(ms);
	while (!done && clk::now() < deadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	return done;
}

static size_t my_list(size_t lists)
{
	const uint64_t tp = (uint64_t)(uintptr_t)__builtin_thread_pointer();
	return (size_t)(((tp >> 4) * 0x9e3779b97f4a7c15ULL) >> 32) % lists;
}

// 1. The state a delete leaves when its writer dies (or is descheduled)
// between clearing USED and unlinking: the element is still linked, not in
// use. A lookup of a key behind it, or a miss in its bucket, must return.
static int case_half_delete()
{
	segment s(1 << 20);
	bpftime_hash_map m(s.mem, 64, 4, 8);
	uint32_t k1 = 1, k2 = 2;
	while (m.bucket_of_key(&k2) != m.bucket_of_key(&k1))
		k2++;
	uint32_t miss = k2 + 1;
	while (m.bucket_of_key(&miss) != m.bucket_of_key(&k1))
		miss++;
	uint64_t v1 = 11, v2 = 22;
	m.elem_update(&k1, &v1);
	m.elem_update(&k2, &v2); // chain: k2 -> k1
	// value at round_up(8 + 4, 8) = 16 from the element start, word at 0
	auto *word = reinterpret_cast<std::atomic<uint32_t> *>(
		(uint8_t *)m.elem_lookup(&k2) - 16);
	const uint32_t w = word->load();
	word->store((w & ~1u) + 2); // delete's first store only
	std::thread t1, t2;
	std::atomic<bool> d1{ false }, d2{ false };
	void *r1 = nullptr, *r2 = (void *)1;
	bool hit = returns_within(
		2000, [&] { r1 = m.elem_lookup(&k1); }, t1, d1);
	bool missed = returns_within(
		2000, [&] { r2 = m.elem_lookup(&miss); }, t2, d2);
	printf("half delete: lookup of the key behind it %s, miss in the bucket %s\n",
	       hit ? "returned" : "did NOT return in 2 s",
	       missed ? "returned" : "did NOT return in 2 s");
	word->store(w); // let them finish
	t1.join();
	t2.join();
	printf("  after restoring the word: hit %s, miss %s\n",
	       r1 && *(uint64_t *)r1 == 11 ? "ok" : "wrong",
	       r2 == nullptr ? "ok" : "wrong");
	return hit && missed ? 0 : 1;
}

// 2. A free list held by a dead process (simulated with the test lock), every
// other list empty: an insert blocks while holding its stripe, and so do the
// stripe's other writers; lookups of the stripe still return.
static int case_dead_list()
{
	segment s(1 << 22);
	const size_t lists = 6, max = 600;
	bpftime_hash_map m(s.mem, max, 4, 8);
	std::atomic<size_t> mine{ lists };
	std::atomic<uint32_t> current{ 0 };
	std::atomic<size_t> inserted{ 0 };
	std::atomic<bool> go{ false }, a_done{ false };
	std::atomic<int> a_last{ -1 };
	// one key already present, to look up later
	uint32_t present = 0xffffff;
	uint64_t v = 1;
	m.elem_update(&present, &v);
	std::thread a([&] {
		mine = my_list(lists);
		while (!go)
			;
		for (uint32_t k = 1;; k++) {
			current = k;
			result r = m.update(&k, &v, mode::no_exist);
			a_last = (int)r;
			if (r != result::ok)
				break;
			inserted++;
		}
		a_done = true;
	});
	while (mine == lists)
		;
	const size_t held = (mine + 1) % lists;
	m.lock_free_list_for_test(held);
	go = true;
	size_t prev = SIZE_MAX;
	for (int i = 0; i < 40 && !a_done; i++) {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		if (inserted == prev && i > 4)
			break;
		prev = inserted;
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(300));
	const bool a_blocked = !a_done;
	printf("dead list holder: list %zu held, inserter's list %zu; inserter %s after %zu inserts (map %zu of %zu)\n",
	       held, (size_t)mine,
	       a_blocked ? "BLOCKED" : (a_last == (int)result::full ? "got E2BIG" : "returned"),
	       (size_t)inserted, (size_t)inserted + 1, max);
	int rc = 0;
	if (a_blocked) {
		const uint32_t ak = current;
		uint32_t bk = ak + 100000;
		while (m.stripe_of_key(&bk) != m.stripe_of_key(&ak))
			bk++;
		uint32_t ck = 0xfffff0;
		std::thread tb, tc;
		std::atomic<bool> db{ false }, dc{ false };
		bool b_ret = returns_within(
			1000, [&] { m.update(&bk, &v, mode::no_exist); }, tb, db);
		// a lookup in the blocked stripe
		while (m.stripe_of_key(&ck) != m.stripe_of_key(&ak))
			ck--;
		bool c_ret = returns_within(
			1000, [&] { m.elem_lookup(&ck); }, tc, dc);
		printf("  another writer of the inserter's stripe %s; a lookup there %s\n",
		       b_ret ? "returned" : "BLOCKED",
		       c_ret ? "returned" : "BLOCKED");
		m.unlock_free_list_for_test(held);
		tb.join();
		tc.join();
		rc = 1;
	} else {
		m.unlock_free_list_for_test(held);
	}
	a.join();
	printf("  after releasing the list: inserter done, %zu inserts, last result %d\n",
	       (size_t)inserted, (int)a_last);
	return rc;
}

// 3. Existing-key updates overwrite the value in place: readers of a 16 B value
// written as {x, x} count mixed values.
static int case_torn_update(int seconds)
{
	segment s(1 << 20);
	bpftime_hash_map m(s.mem, 16, 4, 16);
	uint32_t key = 7;
	uint64_t init[2] = { 0, 0 };
	m.elem_update(&key, init);
	std::atomic<bool> stop{ false };
	std::atomic<uint64_t> torn{ 0 }, reads{ 0 }, torn_copy{ 0 };
	std::vector<std::thread> t;
	for (int w = 0; w < 2; w++)
		t.emplace_back([&, w] {
			for (uint64_t i = 1; !stop; i++) {
				uint64_t x = i * 2 + w;
				uint64_t val[2] = { x, x };
				m.elem_update(&key, val);
			}
		});
	for (int r = 0; r < 3; r++)
		t.emplace_back([&] {
			uint64_t n = 0, bad = 0, badc = 0;
			while (!stop) {
				auto *p = (volatile uint64_t *)m.elem_lookup(&key);
				uint64_t a = p[0], b = p[1];
				bad += a != b;
				// as a BPF program copying the value out
				uint64_t c[2];
				m.lookup_read(&key, [&](const void *vp) {
					memcpy(c, vp, 16);
				});
				badc += c[0] != c[1];
				n++;
			}
			torn += bad;
			torn_copy += badc;
			reads += n;
		});
	std::this_thread::sleep_for(std::chrono::seconds(seconds));
	stop = true;
	for (auto &x : t)
		x.join();
	printf("in-place update: %llu of %llu reads through the pointer mixed, %llu through lookup_read copies\n",
	       (unsigned long long)torn, (unsigned long long)reads,
	       (unsigned long long)torn_copy);
	return torn || torn_copy ? 1 : 0;
}

// 5. Six threads keep (max - slack) keys in total and each does delete own
// oldest + insert new: there is always a free element, so every E2BIG is
// spurious.
static int case_spurious_full(int seconds, size_t slack)
{
	segment s(1 << 22);
	const size_t max = 1024, threads = 6;
	bpftime_hash_map m(s.mem, max, 4, 8);
	const size_t live = (max - slack) / threads;
	std::atomic<bool> stop{ false };
	std::atomic<uint64_t> fulls{ 0 }, ops{ 0 };
	std::vector<std::thread> t;
	for (size_t th = 0; th < threads; th++)
		t.emplace_back([&, th] {
			std::vector<uint32_t> ring(live);
			uint64_t v = 1, next = 0, n = 0, f = 0;
			auto key = [&](uint64_t i) {
				return (uint32_t)(th << 28 | (i & 0xfffffff));
			};
			for (size_t i = 0; i < live; i++) {
				ring[i] = key(next++);
				if (m.update(&ring[i], &v, mode::no_exist) !=
				    result::ok)
					f++;
			}
			for (size_t i = 0; !stop; i = (i + 1) % live) {
				m.elem_delete(&ring[i]);
				ring[i] = key(next++);
				if (m.update(&ring[i], &v, mode::no_exist) !=
				    result::ok)
					f++;
				n++;
			}
			fulls += f;
			ops += n;
		});
	std::this_thread::sleep_for(std::chrono::seconds(seconds));
	stop = true;
	for (auto &x : t)
		x.join();
	printf("mixed delete+insert, %zu live of %zu (%zu free at all times): %llu E2BIG in %llu inserts\n",
	       live * threads, max, max - live * threads,
	       (unsigned long long)fulls, (unsigned long long)ops);
	return fulls ? 1 : 0;
}

// 5b. Inserting threads never delete (their own lists stay empty, every insert
// steals) and deleting threads never insert. An insert starts only after a
// delete has completed for it, so at least slack elements are free at every
// moment: every E2BIG is spurious.
static int case_spurious_full_split(int seconds, size_t slack)
{
	segment s(1 << 22);
	const size_t max = 1024, inserters = 3, deleters = 3;
	bpftime_hash_map m(s.mem, max, 4, 8);
	std::mutex qm;
	std::deque<uint32_t> q;
	std::atomic<uint32_t> next_key{ 1 };
	std::atomic<int64_t> budget{ 0 };
	uint64_t v = 1;
	for (size_t i = 0; i < max - slack; i++) {
		uint32_t k = next_key++;
		m.update(&k, &v, mode::no_exist);
		q.push_back(k);
	}
	std::atomic<bool> stop{ false };
	std::atomic<uint64_t> fulls{ 0 }, ops{ 0 };
	std::vector<std::thread> t;
	for (size_t i = 0; i < inserters; i++)
		t.emplace_back([&] {
			uint64_t n = 0, f = 0;
			while (!stop) {
				int64_t b = budget.load();
				if (b <= 0 || !budget.compare_exchange_weak(b, b - 1))
					continue;
				uint32_t k = next_key++;
				if (m.update(&k, &v, mode::no_exist) == result::ok) {
					std::lock_guard g(qm);
					q.push_back(k);
				} else {
					f++;
					budget++; // nothing was taken
				}
				n++;
			}
			fulls += f;
			ops += n;
		});
	for (size_t i = 0; i < deleters; i++)
		t.emplace_back([&] {
			while (!stop) {
				uint32_t k;
				{
					std::lock_guard g(qm);
					if (q.empty())
						continue;
					k = q.front();
					q.pop_front();
				}
				m.elem_delete(&k);
				budget++;
			}
		});
	std::this_thread::sleep_for(std::chrono::seconds(seconds));
	stop = true;
	for (auto &x : t)
		x.join();
	printf("split inserters/deleters, at least %zu of %zu free at all times: %llu E2BIG in %llu inserts\n",
	       slack, max, (unsigned long long)fulls, (unsigned long long)ops);
	return fulls ? 1 : 0;
}

// 6. Keys k_j = j * phi^-1 (mod 2^64) multiply to j: all land in bucket 0.
static int case_collide()
{
	segment s(1 << 22);
	const size_t max = 4096;
	bpftime_hash_map m(s.mem, max, 8, 8);
	const uint64_t phi = 0x9e3779b97f4a7c15ULL;
	uint64_t inv = phi; // Newton: inv = inv * (2 - phi * inv)
	for (int i = 0; i < 6; i++)
		inv *= 2 - phi * inv;
	uint64_t v = 1;
	size_t n = 1000;
	auto t0 = clk::now();
	for (uint64_t j = 0; j < n; j++) {
		uint64_t k = j * inv;
		m.elem_update(&k, &v);
	}
	auto t1 = clk::now();
	uint64_t k0 = 0;
	uint64_t seq = 12345;
	uint32_t b0 = m.bucket_of_key(&k0);
	printf("colliding keys: %zu keys j * phi^-1, chain of bucket %u has %zu elements; insert %.0f ns each; a sequential key's chain %zu\n",
	       n, b0, m.chain_length_of_key(&k0),
	       std::chrono::duration<double, std::nano>(t1 - t0).count() / n,
	       m.chain_length_of_key(&seq));
	return m.chain_length_of_key(&k0) > 16 ? 1 : 0;
}

int main(int argc, char **argv)
{
	const char *c = argc > 1 ? argv[1] : "";
	if (!strcmp(c, "half-delete"))
		return case_half_delete();
	if (!strcmp(c, "dead-list"))
		return case_dead_list();
	if (!strcmp(c, "torn"))
		return case_torn_update(argc > 2 ? atoi(argv[2]) : 5);
	if (!strcmp(c, "spurious-full"))
		return case_spurious_full(argc > 2 ? atoi(argv[2]) : 5,
					  argc > 3 ? atoi(argv[3]) : 1);
	if (!strcmp(c, "spurious-full-split"))
		return case_spurious_full_split(argc > 2 ? atoi(argv[2]) : 5,
						argc > 3 ? atoi(argv[3]) : 1);
	if (!strcmp(c, "collide"))
		return case_collide();
	fprintf(stderr, "usage: %s half-delete|dead-list|torn [s]|spurious-full [s] [slack]|collide\n",
		argv[0]);
	return 2;
}

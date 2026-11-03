// The hash map's chains, stripe locks and free lists: writers lock only the
// key's stripe, lookups no lock at all; lookups of keys present throughout
// never miss while elements around them are freed and reused in other
// chains, also while they are updated; inserts take free elements from other threads' lists, so capacity
// stays exactly max_entries.
#include <bpf_map/bpftime_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

using namespace boost::interprocess;
using namespace bpftime;

namespace
{
struct shm_guard {
	const char *name;
	managed_shared_memory segment;
	explicit shm_guard(const char *name)
		: name((shared_memory_object::remove(name), name)),
		  segment(create_only, name, 16 << 20)
	{
	}
	~shm_guard()
	{
		shared_memory_object::remove(name);
	}
};

// Whether op, run on another thread, waited for lock() until unlock().
template <class Lock, class Unlock, class Op>
bool waits(Lock lock, Unlock unlock, Op op)
{
	lock();
	std::atomic<bool> done{ false };
	std::thread runner([&] {
		op();
		done.store(true);
	});
	for (int i = 0; i < 100 && !done.load(); i++)
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	const bool waited = !done.load();
	unlock();
	runner.join();
	return waited;
}
} // namespace

TEST_CASE("hash map writers lock only the key's stripe",
	  "[bpftime_hash_map][chains]")
{
	shm_guard shm("bpftime_hash_chains_stripe_test");
	bpftime_hash_map map(shm.segment, 1024, 4, 8);
	REQUIRE(map.stripe_count() > 2);
	const uint32_t key = 7;
	const uint64_t one = 1, two = 2;
	REQUIRE(map.elem_update(&key, &one));
	const size_t s = map.stripe_of_key(&key);
	const size_t other = (s + 1) % map.stripe_count();
	auto hold = [&](size_t stripe) {
		return std::make_pair([&map, stripe] { map.lock_stripe_for_test(stripe); },
				      [&map, stripe] { map.unlock_stripe_for_test(stripe); });
	};
	auto [lock_s, unlock_s] = hold(s);
	auto [lock_o, unlock_o] = hold(other);
	auto update = [&] { REQUIRE(map.elem_update(&key, &two)); };
	auto erase = [&] { REQUIRE(map.elem_delete(&key)); };
	auto insert = [&] { REQUIRE(map.elem_update(&key, &one)); };
	auto lookup = [&] { REQUIRE(map.elem_lookup(&key) != nullptr); };
	REQUIRE(waits(lock_s, unlock_s, update));
	REQUIRE_FALSE(waits(lock_o, unlock_o, update));
	REQUIRE_FALSE(waits(lock_s, unlock_s, lookup));
	REQUIRE(waits(lock_s, unlock_s, erase));
	REQUIRE_FALSE(waits(lock_o, unlock_o, insert));
	// Lookups and updates of an existing key take no free-list lock.
	auto hold_lists = [&] {
		for (size_t l = 0; l < map.free_list_count(); l++)
			map.lock_free_list_for_test(l);
	};
	auto release_lists = [&] {
		for (size_t l = 0; l < map.free_list_count(); l++)
			map.unlock_free_list_for_test(l);
	};
	REQUIRE_FALSE(waits(hold_lists, release_lists, lookup));
	REQUIRE_FALSE(waits(hold_lists, release_lists, update));
	// A delete gives its element back to a free list.
	REQUIRE(waits(hold_lists, release_lists, erase));
}

// Present keys share buckets with churn keys; writers delete and insert the
// churn keys, so elements leave these chains, pass through the free lists
// and come back in other chains while readers walk past them. A lookup that
// follows a freed element must start again, never end as a miss. (Updates of
// present keys: "hash map lookups of keys updated at once never miss".)
TEST_CASE("hash map lookups never miss keys present while their chains churn",
	  "[bpftime_hash_map][chains][concurrency]")
{
	shm_guard shm("bpftime_hash_chains_churn_test");
	constexpr size_t max_entries = 256;
	bpftime_hash_map map(shm.segment, max_entries, 4, 8);
	// 16 buckets: 2 present keys each, then churn keys of the same buckets
	// (first in each chain's walk order, so readers pass them).
	std::vector<uint32_t> present, churn;
	std::vector<int> per_bucket(map.bucket_count());
	for (uint32_t key = 1; present.size() < 32 && key < (1u << 24); key++) {
		const uint32_t b = map.bucket_of_key(&key);
		if (b < 16 && per_bucket[b] < 2) {
			per_bucket[b]++;
			present.push_back(key);
		}
	}
	REQUIRE(present.size() == 32);
	for (uint32_t key : present) {
		const uint64_t value = key;
		REQUIRE(map.elem_update(&key, &value));
	}
	for (uint32_t key = 1u << 24; churn.size() < 180; key++) {
		const uint32_t b = map.bucket_of_key(&key);
		if (b < 16)
			churn.push_back(key);
	}
	// Some elements live in other buckets' chains, so freed elements are
	// reused there.
	std::vector<uint32_t> elsewhere;
	for (uint32_t key = 1u << 25; elsewhere.size() < 30; key++)
		if (map.bucket_of_key(&key) >= 16)
			elsewhere.push_back(key);
	std::atomic<bool> stop{ false };
	std::atomic<uint64_t> misses{ 0 }, wrong{ 0 }, lookups{ 0 };
	std::vector<std::thread> threads;
	constexpr int writers = 3, readers = 2;
	for (int w = 0; w < writers; w++)
		threads.emplace_back([&, w] {
			const uint64_t value = 0;
			while (!stop.load(std::memory_order_relaxed)) {
				for (size_t i = w; i < churn.size(); i += writers)
					map.elem_update(&churn[i], &value);
				for (size_t i = w; i < elsewhere.size(); i += writers)
					map.elem_update(&elsewhere[i], &value);
				for (size_t i = w; i < churn.size(); i += writers)
					map.elem_delete(&churn[i]);
				for (size_t i = w; i < elsewhere.size(); i += writers)
					map.elem_delete(&elsewhere[i]);
			}
		});
	for (int r = 0; r < readers; r++)
		threads.emplace_back([&] {
			uint64_t miss = 0, bad = 0, n = 0;
			while (!stop.load(std::memory_order_relaxed)) {
				for (uint32_t key : present) {
					const auto *v = static_cast<const uint64_t *>(
						map.elem_lookup(&key));
					n++;
					if (v == nullptr)
						miss++;
					else if (std::atomic_ref<const uint64_t>(*v).load(
							 std::memory_order_relaxed) != key)
						bad++;
				}
			}
			misses += miss;
			wrong += bad;
			lookups += n;
		});
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
	stop = true;
	for (auto &t : threads)
		t.join();
	INFO("lookups " << lookups.load());
	REQUIRE(lookups.load() > 100000);
	REQUIRE(misses.load() == 0);
	REQUIRE(wrong.load() == 0);
}

// One thread's own list holds only its share of the elements: inserts beyond
// it take free elements from the other lists, up to exactly max_entries.
TEST_CASE("hash map inserts take free elements from every list",
	  "[bpftime_hash_map][chains][capacity]")
{
	shm_guard shm("bpftime_hash_chains_lists_test");
	constexpr uint32_t max_entries = 1000;
	bpftime_hash_map map(shm.segment, max_entries, 4, 8);
	const uint64_t value = 1;
	for (int round = 0; round < 3; round++) {
		for (uint32_t key = 0; key < max_entries; key++)
			REQUIRE(map.elem_update(&key, &value));
		REQUIRE(map.get_elem_count() == max_entries);
		const uint32_t extra = max_entries;
		REQUIRE(map.update(&extra, &value,
				   bpftime_hash_map::update_mode::any) ==
			bpftime_hash_map::update_result::full);
		// Deleted on other threads: the elements go to their lists.
		std::vector<std::thread> threads;
		for (int t = 0; t < 4; t++)
			threads.emplace_back([&, t] {
				for (uint32_t key = t; key < max_entries; key += 4)
					REQUIRE(map.elem_delete(&key));
			});
		for (auto &t : threads)
			t.join();
		REQUIRE(map.get_elem_count() == 0);
	}
}

// A deleter that stops between its two stores (preempted, or killed: then
// for good) must not make lookups of that chain restart. Keys behind the
// element and misses in its bucket are looked up in a child process, so that
// a lookup that spins can be killed.
TEST_CASE("hash map lookups pass a delete stopped halfway",
	  "[bpftime_hash_map][chains][delete]")
{
	shm_guard shm("bpftime_hash_chains_half_delete_test");
	bpftime_hash_map map(shm.segment, 16, 4, 8);
	// Three keys of one bucket: behind is inserted first, so the deleted
	// key is ahead of it in the chain; absent is never inserted.
	std::vector<uint32_t> same;
	for (uint32_t key = 1; same.size() < 3; key++) {
		uint32_t first = same.empty() ? key : same[0];
		if (map.bucket_of_key(&key) == map.bucket_of_key(&first))
			same.push_back(key);
	}
	const uint32_t behind = same[0], deleted = same[1], absent = same[2];
	const uint64_t value = 5;
	REQUIRE(map.elem_update(&behind, &value));
	REQUIRE(map.elem_update(&deleted, &value));
	REQUIRE(map.chain_length_of_key(&behind) == 2);
	REQUIRE(map.delete_first_store_for_test(&deleted));
	// The first store is the unlink.
	REQUIRE(map.chain_length_of_key(&behind) == 1);
	const pid_t child = fork();
	REQUIRE(child >= 0);
	if (child == 0) {
		const auto *v = static_cast<const uint64_t *>(
			map.elem_lookup(&behind));
		const bool ok = v != nullptr && *v == value &&
				map.elem_lookup(&absent) == nullptr &&
				map.elem_lookup(&deleted) == nullptr;
		_exit(ok ? 0 : 1);
	}
	int status = 0;
	pid_t done = 0;
	for (int i = 0; i < 200 && done == 0; i++) {
		done = waitpid(child, &status, WNOHANG);
		if (done == 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	if (done == 0) {
		kill(child, SIGKILL);
		waitpid(child, &status, 0);
	}
	REQUIRE(done == child);
	REQUIRE(WIFEXITED(status));
	REQUIRE(WEXITSTATUS(status) == 0);
	// The element is leaked: out of the chain, in no free list.
	map.unlock_stripe_for_test(map.stripe_of_key(&deleted));
	REQUIRE(map.get_elem_count() == 2);
	REQUIRE(map.elem_lookup(&behind) != nullptr);
}

// Keys computed to share one chain under the unseeded hash: 8-byte keys
// j * phi^-1 (their products with phi are 0, 1, 2, ...: one top-bits
// bucket), and 16-byte keys whose second word cancels the mix of the first.
// With the per-map seed they spread like any keys.
TEST_CASE("hash map chains stay short for keys made to collide",
	  "[bpftime_hash_map][chains][seed]")
{
	constexpr size_t max_entries = 4096, n = 1000;
	const uint64_t phi = 0x9e3779b97f4a7c15ULL, mix = 0xbf58476d1ce4e5b9ULL;
	uint64_t inv = phi; // Newton: inv = inv * (2 - phi * inv)
	for (int i = 0; i < 6; i++)
		inv *= 2 - phi * inv;
	REQUIRE(inv * phi == 1);
	const uint64_t value = 1;
	SECTION("8-byte keys")
	{
		shm_guard shm("bpftime_hash_chains_seed8_test");
		bpftime_hash_map map(shm.segment, max_entries, 8, 8);
		size_t longest = 0;
		for (uint64_t j = 0; j < n; j++) {
			const uint64_t key = j * inv;
			REQUIRE(map.elem_update(&key, &value));
		}
		for (uint64_t j = 0; j < n; j++) {
			const uint64_t key = j * inv;
			longest = std::max(longest, map.chain_length_of_key(&key));
		}
		INFO("longest chain " << longest);
		REQUIRE(longest <= 8);
	}
	SECTION("16-byte keys")
	{
		shm_guard shm("bpftime_hash_chains_seed16_test");
		bpftime_hash_map map(shm.segment, max_entries, 16, 8);
		auto make = [&](uint64_t j, uint64_t *key) {
			uint64_t h = (16 * phi ^ j) * mix;
			h ^= h >> 31;
			key[0] = j;
			key[1] = h ^ 0x1234;
		};
		size_t longest = 0;
		for (uint64_t j = 0; j < n; j++) {
			uint64_t key[2];
			make(j, key);
			REQUIRE(map.elem_update(key, &value));
		}
		for (uint64_t j = 0; j < n; j++) {
			uint64_t key[2];
			make(j, key);
			longest = std::max(longest, map.chain_length_of_key(key));
		}
		INFO("longest chain " << longest);
		REQUIRE(longest <= 8);
	}
}

// Fewer entries than free lists: most lists start empty, and an insert finds
// the few elements wherever they are, up to exactly max_entries.
TEST_CASE("hash map with fewer entries than free lists",
	  "[bpftime_hash_map][chains][capacity]")
{
	shm_guard shm("bpftime_hash_chains_tiny_test");
	bpftime_hash_map map(shm.segment, 2, 4, 8);
	INFO("free lists " << map.free_list_count());
	const uint64_t value = 3;
	for (int round = 0; round < 3; round++) {
		const uint32_t a = 10 + round, b = 20 + round, c = 30;
		REQUIRE(map.elem_update(&a, &value));
		REQUIRE(map.elem_update(&b, &value));
		REQUIRE(map.update(&c, &value, bpftime_hash_map::update_mode::any) ==
			bpftime_hash_map::update_result::full);
		REQUIRE(map.get_elem_count() == 2);
		REQUIRE(map.elem_delete(&a));
		REQUIRE(map.elem_delete(&b));
		REQUIRE(map.get_elem_count() == 0);
	}
}

// lookup_read with 8- and 16-byte keys (the memcmp path): read runs on the
// key's own value, a miss does not call it.
TEST_CASE("hash map lookup_read with longer keys",
	  "[bpftime_hash_map][chains]")
{
	for (size_t key_size : { size_t{ 8 }, size_t{ 16 } }) {
		shm_guard shm("bpftime_hash_chains_read_test");
		bpftime_hash_map map(shm.segment, 64, key_size, 8);
		for (uint64_t i = 0; i < 64; i++) {
			uint64_t key[2] = { i, ~i };
			const uint64_t value = i * 7;
			REQUIRE(map.elem_update(key, &value));
		}
		for (uint64_t i = 0; i < 64; i++) {
			uint64_t key[2] = { i, ~i };
			uint64_t seen = ~0ull;
			const void *p = map.lookup_read(key, [&](const void *v) {
				std::memcpy(&seen, v, 8);
			});
			REQUIRE(p != nullptr);
			REQUIRE(seen == i * 7);
		}
		uint64_t missing[2] = { 1000, 0 };
		bool called = false;
		REQUIRE(map.lookup_read(missing, [&](const void *) {
			called = true;
		}) == nullptr);
		REQUIRE_FALSE(called);
	}
}

// An update of an existing key puts the new value in a spare element: the
// map stays exact (spares are not capacity), a full map still takes updates,
// and a key alone in its chain (its element the head) stays findable.
TEST_CASE("hash map updates of existing keys replace their element",
	  "[bpftime_hash_map][chains][replace]")
{
	shm_guard shm("bpftime_hash_chains_replace_test");
	constexpr uint32_t max_entries = 64;
	bpftime_hash_map map(shm.segment, max_entries, 4, 8);
	const uint32_t key = 5;
	uint64_t value = 1;
	REQUIRE(map.elem_update(&key, &value));
	const void *first = map.elem_lookup(&key);
	value = 2;
	REQUIRE(map.elem_update(&key, &value));
	// A new element (one thread alternates between it and the spare).
	REQUIRE(map.elem_lookup(&key) != first);
	REQUIRE(*static_cast<const uint64_t *>(first) == 1);
	for (value = 3; value < 1000; value++) {
		REQUIRE(map.elem_update(&key, &value));
		REQUIRE(map.chain_length_of_key(&key) == 1);
		const auto *v =
			static_cast<const uint64_t *>(map.elem_lookup(&key));
		REQUIRE(v != nullptr);
		REQUIRE(*v == value);
	}
	uint32_t next = 0;
	REQUIRE(map.next_key(nullptr, &next));
	REQUIRE(next == key);
	REQUIRE(map.get_elem_count() == 1);
	// Full: every key can still be updated, nothing more inserted.
	for (uint32_t k = 100; map.get_elem_count() < max_entries; k++)
		REQUIRE(map.elem_update(&k, &value));
	const uint32_t extra = 1u << 20;
	REQUIRE(map.update(&extra, &value, bpftime_hash_map::update_mode::any) ==
		bpftime_hash_map::update_result::full);
	for (int round = 0; round < 3; round++)
		for (uint32_t k = 100; k < 100 + max_entries - 1; k++) {
			const uint64_t v = k + round;
			REQUIRE(map.update(&k, &v,
					   bpftime_hash_map::update_mode::exist) ==
				bpftime_hash_map::update_result::ok);
			REQUIRE(*static_cast<const uint64_t *>(
					map.elem_lookup(&k)) == v);
		}
	REQUIRE(map.get_elem_count() == max_entries);
	REQUIRE(map.update(&extra, &value, bpftime_hash_map::update_mode::any) ==
		bpftime_hash_map::update_result::full);
}

// Two updaters write {x, x} into one key's 16-byte value; copies taken by
// lookup_read must never mix two values (in place they did: ~1 million in
// 5 s). Reads through a pointer can still mix when the old element is
// reused by a later update while the reader holds it, as with the kernel's
// extra_elems; those are only counted.
TEST_CASE("hash map copies of a value being updated are whole",
	  "[bpftime_hash_map][chains][replace][concurrency]")
{
	shm_guard shm("bpftime_hash_chains_torn_test");
	bpftime_hash_map map(shm.segment, 64, 4, 16);
	const uint32_t key = 9;
	const uint64_t init[2] = { 0, 0 };
	REQUIRE(map.elem_update(&key, init));
	std::atomic<bool> stop{ false };
	std::atomic<uint64_t> torn_copies{ 0 }, torn_pointer{ 0 }, copies{ 0 };
	std::vector<std::thread> threads;
	for (int w = 0; w < 2; w++)
		threads.emplace_back([&, w] {
			for (uint64_t x = w; !stop.load(std::memory_order_relaxed);
			     x += 2) {
				const uint64_t value[2] = { x, x };
				map.elem_update(&key, value);
			}
		});
	for (int r = 0; r < 3; r++)
		threads.emplace_back([&, r] {
			uint64_t torn = 0, ptorn = 0, n = 0;
			while (!stop.load(std::memory_order_relaxed)) {
				uint64_t a = 0, b = 0;
				const void *p = map.lookup_read(&key, [&](const void *v) {
					auto *q = static_cast<uint64_t *>(
						const_cast<void *>(v));
					a = std::atomic_ref<uint64_t>(q[0]).load(
						std::memory_order_acquire);
					b = std::atomic_ref<uint64_t>(q[1]).load(
						std::memory_order_acquire);
				});
				if (p == nullptr)
					continue;
				n++;
				if (a != b)
					torn++;
				auto *q = static_cast<uint64_t *>(
					const_cast<void *>(p));
				if (std::atomic_ref<uint64_t>(q[0]).load(
					    std::memory_order_relaxed) !=
				    std::atomic_ref<uint64_t>(q[1]).load(
					    std::memory_order_relaxed))
					ptorn++;
			}
			torn_copies += torn;
			torn_pointer += ptorn;
			copies += n;
		});
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
	stop = true;
	for (auto &t : threads)
		t.join();
	INFO("copies " << copies.load() << ", torn copies " << torn_copies.load()
		       << ", torn pointer reads " << torn_pointer.load());
	REQUIRE(copies.load() > 100000);
	REQUIRE(torn_copies.load() == 0);
	const auto *v = static_cast<const uint64_t *>(map.elem_lookup(&key));
	REQUIRE(v != nullptr);
	REQUIRE(v[0] == v[1]);
	REQUIRE(map.get_elem_count() == 1);
}

// Two keys of one bucket updated at once, so that one of them is replaced
// while it is not the head: its new element takes its place in the chain and
// the old one is retired and reused by the next update of the stripe.
// Lookups of either key must never miss (both are present throughout) and
// copies must hold their own key's value, whole. Linked at the head instead
// (the kernel's way), a lookup on the old element's predecessor reached the
// end: on the NUC 0.001-0.31% of these lookups missed (the kernel's own map
// up to 0.63%), on the Orin 4-8 in 61 million.
TEST_CASE("hash map lookups of keys updated at once never miss",
	  "[bpftime_hash_map][chains][replace][concurrency]")
{
	shm_guard shm("bpftime_hash_chains_behind_test");
	// Values {key, x, x}: a copy must hold its own key's value, whole.
	bpftime_hash_map map(shm.segment, 64, 4, 24);
	uint32_t keys[2] = { 1, 0 };
	for (uint32_t k = 2; keys[1] == 0; k++)
		if (map.bucket_of_key(&k) == map.bucket_of_key(&keys[0]))
			keys[1] = k;
	for (const uint32_t &key : keys) {
		const uint64_t init[3] = { key, 0, 0 };
		REQUIRE(map.elem_update(&key, init));
	}
	REQUIRE(map.chain_length_of_key(&keys[0]) == 2);
	std::atomic<bool> stop{ false };
	std::atomic<uint64_t> misses{ 0 }, torn{ 0 }, reads{ 0 };
	std::vector<std::thread> threads;
	for (int w = 0; w < 2; w++)
		threads.emplace_back([&, w] {
			for (uint64_t x = 1; !stop.load(std::memory_order_relaxed);
			     x++) {
				const uint64_t value[3] = { keys[w], x, x };
				map.elem_update(&keys[w], value);
			}
		});
	for (int r = 0; r < 3; r++)
		threads.emplace_back([&, r] {
			uint64_t miss = 0, bad = 0, n = 0;
			while (!stop.load(std::memory_order_relaxed)) {
				for (const uint32_t &key : keys) {
					uint64_t k = 0, a = 0, b = 0;
					const void *p = map.lookup_read(
						&key, [&](const void *v) {
							auto *q = static_cast<uint64_t *>(
								const_cast<void *>(v));
							k = std::atomic_ref<uint64_t>(q[0]).load(
								std::memory_order_acquire);
							a = std::atomic_ref<uint64_t>(q[1]).load(
								std::memory_order_acquire);
							b = std::atomic_ref<uint64_t>(q[2]).load(
								std::memory_order_acquire);
						});
					n++;
					if (p == nullptr)
						miss++;
					else if (k != key || a != b)
						bad++;
				}
			}
			misses += miss;
			torn += bad;
			reads += n;
		});
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
	stop = true;
	for (auto &t : threads)
		t.join();
	INFO("reads " << reads.load() << ", misses " << misses.load()
		      << ", torn " << torn.load());
	REQUIRE(reads.load() > 100000);
	REQUIRE(misses.load() == 0);
	REQUIRE(torn.load() == 0);
	REQUIRE(map.get_elem_count() == 2);
}

// A lookup stands on a key's element, behind the head, when the key is
// updated (the element is unlinked and becomes the stripe's spare) and then
// another key of the stripe is updated (the spare is reused for it): the
// lookup must not return the other key's value. The read callback runs after
// the key matched and before the word is checked again; it does both
// updates the first time.
TEST_CASE("hash map lookups never return a value reused for another key",
	  "[bpftime_hash_map][chains][replace]")
{
	shm_guard shm("bpftime_hash_chains_reuse_test");
	bpftime_hash_map map(shm.segment, 64, 4, 8);
	uint32_t other = 1, key = 0;
	for (uint32_t k = 2; key == 0; k++)
		if (map.bucket_of_key(&k) == map.bucket_of_key(&other))
			key = k;
	// key first, so other is the head and key's element is behind it.
	uint64_t value = 100;
	REQUIRE(map.elem_update(&key, &value));
	value = 200;
	REQUIRE(map.elem_update(&other, &value));
	int calls = 0;
	uint64_t seen = 0;
	const void *p = map.lookup_read(&key, [&](const void *v) {
		if (calls++ == 0) {
			const uint64_t mine = 101, theirs = 201;
			REQUIRE(map.elem_update(&key, &mine));
			REQUIRE(map.elem_update(&other, &theirs));
			// The element the lookup stands on now holds other.
			REQUIRE(*static_cast<const uint64_t *>(v) == 201);
		}
		seen = std::atomic_ref<uint64_t>(
			       *static_cast<uint64_t *>(const_cast<void *>(v)))
			       .load(std::memory_order_acquire);
	});
	REQUIRE(p != nullptr);
	REQUIRE(calls == 2);
	REQUIRE(seen == 101);
}

// An update puts the new element where the old one was: the chain keeps its
// order (next_key returns the same sequence), and a key behind the head stays
// behind it. Linked at the head, the updated key moved to the front, so
// next_key during updates returned keys again and could cycle between two
// keys of a bucket.
TEST_CASE("hash map updates keep the chain's order",
	  "[bpftime_hash_map][chains][replace]")
{
	shm_guard shm("bpftime_hash_chains_order_test");
	bpftime_hash_map map(shm.segment, 64, 4, 8);
	std::vector<uint32_t> same{ 1 };
	for (uint32_t k = 2; same.size() < 4; k++)
		if (map.bucket_of_key(&k) == map.bucket_of_key(&same[0]))
			same.push_back(k);
	uint64_t value = 0;
	for (uint32_t key : same)
		REQUIRE(map.elem_update(&key, &value));
	auto order = [&] {
		std::vector<uint32_t> keys;
		uint32_t key = 0, next = 0;
		const void *at = nullptr;
		while (map.next_key(at, &next)) {
			keys.push_back(next);
			key = next;
			at = &key;
		}
		return keys;
	};
	const auto before = order();
	REQUIRE(before.size() == 4);
	// The first inserted is last in its chain; then each key in turn, in
	// an order that differs from the chain's.
	for (value = 1; value <= 8; value++) {
		REQUIRE(map.elem_update(&same[0], &value));
		REQUIRE(order() == before);
		for (uint32_t key : { same[2], same[0], same[3], same[1] })
			REQUIRE(map.elem_update(&key, &value));
		REQUIRE(order() == before);
	}
	REQUIRE(map.chain_length_of_key(&same[0]) == 4);
	REQUIRE(map.get_elem_count() == 4);
}

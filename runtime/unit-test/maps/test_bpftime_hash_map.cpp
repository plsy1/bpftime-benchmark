#include "catch2/catch_message.hpp"
#include "../common_def.hpp"
#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <bpf_map/userspace/per_cpu_hash_map.hpp>
#include "bpftime_config.hpp"
#include "bpftime_shm.hpp"
#include "bpftime_handler.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <sched.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <csignal>
#include <unistd.h>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include <bpf_map/map_common_def.hpp>
#include "catch2/internal/catch_run_context.hpp"
#include <algorithm>
#include <random>
#include <map>
#include <bpf_map/bpftime_hash_map.hpp>

using namespace boost::interprocess;
using namespace bpftime;

TEST_CASE("bpftime_hash_map basic operations", "[bpftime_hash_map]")
{
	shared_memory_object::remove("bpftime_hash_map_test");
	struct remove_at_exit {
		~remove_at_exit()
		{
			shared_memory_object::remove("bpftime_hash_map_test");
		}
	} remover;
	managed_shared_memory segment(open_or_create, "bpftime_hash_map_test",
				      65536);

	size_t num_buckets = 10;
	size_t key_size = 4; // Example key size
	size_t value_size = 8; // Example value size

	bpftime_hash_map map(segment, num_buckets, key_size, value_size);

	SECTION("Insert and Lookup")
	{
		int key1 = 1234;
		int64_t value1 = 5678;
		int key2 = 4321;
		int64_t value2 = 8765;

		REQUIRE(map.elem_update(&key1, &value1) == true);
		REQUIRE(map.elem_update(&key2, &value2) == true);

		REQUIRE(map.elem_lookup(&key1) != nullptr);
		REQUIRE(*(int64_t *)map.elem_lookup(&key1) == value1);

		REQUIRE(map.elem_lookup(&key2) != nullptr);
		REQUIRE(*(int64_t *)map.elem_lookup(&key2) == value2);

		int key3 = 9999;
		REQUIRE(map.elem_lookup(&key3) == nullptr);
	}

	SECTION("Update existing element")
	{
		int key = 1234;
		int64_t value1 = 5678;
		int64_t value2 = 8765;

		REQUIRE(map.elem_update(&key, &value1) == true);
		REQUIRE(map.elem_lookup(&key) != nullptr);
		REQUIRE(*(int64_t *)map.elem_lookup(&key) == value1);

		REQUIRE(map.elem_update(&key, &value2) == true);
		REQUIRE(map.elem_lookup(&key) != nullptr);
		REQUIRE(*(int64_t *)map.elem_lookup(&key) == value2);
	}

	SECTION("Delete element")
	{
		int key1 = 1234;
		int64_t value1 = 5678;
		int key2 = 4321;
		int64_t value2 = 8765;

		REQUIRE(map.elem_update(&key1, &value1) == true);
		REQUIRE(map.elem_update(&key2, &value2) == true);

		REQUIRE(map.elem_delete(&key1) == true);
		REQUIRE(map.elem_lookup(&key1) == nullptr);
		REQUIRE(map.elem_lookup(&key2) != nullptr);
		REQUIRE(*(int64_t *)map.elem_lookup(&key2) == value2);

		REQUIRE(map.elem_delete(&key2) == true);
		REQUIRE(map.elem_lookup(&key2) == nullptr);
	}

	SECTION("Delete preserves a colliding chain")
	{
		// Three keys of one bucket's chain.
		int key1 = 1, key2 = 0, key3 = 0;
		for (int k = 2; key3 == 0; k++) {
			if (map.bucket_of_key(&k) != map.bucket_of_key(&key1))
				continue;
			(key2 == 0 ? key2 : key3) = k;
		}
		int64_t value1 = 101;
		int64_t value2 = 112;
		int64_t value3 = 123;

		REQUIRE(map.elem_update(&key1, &value1) == true);
		REQUIRE(map.elem_update(&key2, &value2) == true);
		REQUIRE(map.elem_delete(&key1) == true);

		auto *second = static_cast<int64_t *>(map.elem_lookup(&key2));
		REQUIRE(second != nullptr);
		REQUIRE(*second == value2);

		// A later insertion may reuse the freed element without making
		// the existing colliding key unreachable.
		REQUIRE(map.elem_update(&key3, &value3) == true);
		second = static_cast<int64_t *>(map.elem_lookup(&key2));
		auto *third = static_cast<int64_t *>(map.elem_lookup(&key3));
		REQUIRE(second != nullptr);
		REQUIRE(*second == value2);
		REQUIRE(third != nullptr);
		REQUIRE(*third == value3);
	}

	SECTION("Get element count")
	{
		int key1 = 1234;
		int64_t value1 = 5678;
		int key2 = 4321;
		int64_t value2 = 8765;

		REQUIRE(map.get_elem_count() == 0);

		REQUIRE(map.elem_update(&key1, &value1) == true);
		REQUIRE(map.get_elem_count() == 1);

		REQUIRE(map.elem_update(&key2, &value2) == true);
		REQUIRE(map.get_elem_count() == 2);

		REQUIRE(map.elem_delete(&key1) == true);
		REQUIRE(map.get_elem_count() == 1);

		REQUIRE(map.elem_delete(&key2) == true);
		REQUIRE(map.get_elem_count() == 0);
	}

	SECTION("Insert more elements than num_buckets")
	{
		int key;
		int64_t value;

		for (size_t i = 0; i < num_buckets; ++i) {
			key = i;
			value = i * 100;
			REQUIRE(map.elem_update(&key, &value) == true);
		}

		key = num_buckets;
		value = num_buckets * 100;
		REQUIRE(map.elem_update(&key, &value) == false); // Should fail
								 // as the map
								 // is full

		for (size_t i = 0; i < num_buckets; ++i) {
			key = i;
			REQUIRE(map.elem_lookup(&key) != nullptr);
			REQUIRE(*(int64_t *)map.elem_lookup(&key) ==
				(int64_t)i * 100);
		}
	}

	SECTION("Insert, delete, and re-insert elements")
	{
		int key1 = 1234;
		int64_t value1 = 5678;
		int key2 = 4321;
		int64_t value2 = 8765;
		int key3 = 5678;
		int64_t value3 = 4321;

		REQUIRE(map.elem_update(&key1, &value1) == true);
		REQUIRE(map.elem_update(&key2, &value2) == true);

		REQUIRE(map.elem_delete(&key1) == true);
		REQUIRE(map.elem_lookup(&key1) == nullptr);

		REQUIRE(map.elem_update(&key3, &value3) == true);
		REQUIRE(map.elem_lookup(&key3) != nullptr);
		REQUIRE(*(int64_t *)map.elem_lookup(&key3) == value3);

		REQUIRE(map.elem_lookup(&key2) != nullptr);
		REQUIRE(*(int64_t *)map.elem_lookup(&key2) == value2);
	}
}

// A tool's lookup-and-delete into a bad buffer used to fault while holding
// the key's stripe lock (the value was copied under it): the dead tool left
// every writer of that stripe in the traced process spinning (review
// 2026-11-05).
TEST_CASE("bpftime_hash_map lookup-and-delete copies out after unlocking",
	  "[bpftime_hash_map]")
{
	shared_memory_object::remove("bpftime_hash_map_delete_out_test");
	managed_shared_memory segment(create_only,
				      "bpftime_hash_map_delete_out_test", 1 << 20);
	struct remove_at_exit {
		~remove_at_exit()
		{
			shared_memory_object::remove(
				"bpftime_hash_map_delete_out_test");
		}
	} remover;
	bpftime_hash_map map(segment, 16, sizeof(uint32_t), sizeof(uint64_t));
	const uint32_t key = 3;
	uint64_t value = 9;
	REQUIRE(map.elem_update(&key, &value));
	void *bad = mmap(nullptr, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS,
			 -1, 0);
	REQUIRE(bad != MAP_FAILED);
	pid_t tool = fork();
	if (tool == 0) {
		signal(SIGSEGV, SIG_DFL); // not Catch's report
		map.elem_delete(&key, bad); // faults
		_exit(0);
	}
	int status = 0;
	REQUIRE(waitpid(tool, &status, 0) == tool);
	REQUIRE(WIFSIGNALED(status));
	// The traced process updates the same key (the same stripe).
	pid_t writer = fork();
	if (writer == 0) {
		alarm(5);
		_exit(map.elem_update(&key, &value) ? 0 : 1);
	}
	REQUIRE(waitpid(writer, &status, 0) == writer);
	INFO("writer " << (WIFSIGNALED(status) ? "killed by signal " : "exit ")
		       << (WIFSIGNALED(status) ? WTERMSIG(status) :
						WEXITSTATUS(status)));
	REQUIRE(WIFEXITED(status));
	REQUIRE(WEXITSTATUS(status) == 0);
	munmap(bad, 4096);
}

// The memcmp key path (8- and 16-byte keys) under replace-in-place storms: two
// keys of one bucket updated at once, a third inserted and deleted, three
// readers. A lookup of either key never misses and copies are whole and the
// key's own. Replacements were tested with 4-byte keys only (review
// 2026-11-05: 0 misses, 0 torn in 45-59M reads).
TEST_CASE("bpftime_hash_map replaces long keys in place without misses",
	  "[bpftime_hash_map]")
{
	for (size_t key_size : { 8, 16 }) {
		CAPTURE(key_size);
		shared_memory_object::remove("bpftime_hash_map_long_keys_test");
		managed_shared_memory segment(create_only,
					      "bpftime_hash_map_long_keys_test",
					      16 << 20);
		struct remove_at_exit {
			~remove_at_exit()
			{
				shared_memory_object::remove(
					"bpftime_hash_map_long_keys_test");
			}
		} remover;
		bpftime_hash_map map(segment, 64, key_size, 24);
		// Keys differing only in their last word, so that memcmp
		// looks past the first 8 bytes of 16-byte keys.
		auto make = [&](uint64_t n) {
			std::vector<uint8_t> k(key_size, 0);
			const uint64_t fixed = 0x1122334455667788ULL;
			if (key_size == 16)
				memcpy(k.data(), &fixed, 8);
			memcpy(k.data() + key_size - 8, &n, 8);
			return k;
		};
		std::vector<std::vector<uint8_t> > keys{ make(1) };
		for (uint64_t n = 2; keys.size() < 3; n++) {
			auto k = make(n);
			if (map.bucket_of_key(k.data()) ==
			    map.bucket_of_key(keys[0].data()))
				keys.push_back(k);
		}
		for (uint64_t i = 0; i < 2; i++) {
			const uint64_t init[3] = { i, 0, 0 };
			REQUIRE(map.elem_update(keys[i].data(), init));
		}
		std::atomic<bool> stop{ false };
		std::atomic<uint64_t> misses{ 0 }, torn{ 0 }, reads{ 0 };
		std::vector<std::thread> threads;
		for (uint64_t w = 0; w < 2; w++)
			threads.emplace_back([&, w] {
				for (uint64_t x = 1;
				     !stop.load(std::memory_order_relaxed); x++) {
					const uint64_t value[3] = { w, x, x };
					map.elem_update(keys[w].data(), value);
				}
			});
		threads.emplace_back([&] {
			const uint64_t value[3] = { 2, 5, 5 };
			while (!stop.load(std::memory_order_relaxed)) {
				map.elem_update(keys[2].data(), value);
				map.elem_delete(keys[2].data());
			}
		});
		for (int r = 0; r < 3; r++)
			threads.emplace_back([&] {
				uint64_t miss = 0, bad = 0, n = 0;
				while (!stop.load(std::memory_order_relaxed)) {
					for (uint64_t i = 0; i < 2; i++, n++) {
						uint64_t k = 0, a = 0, b = 0;
						const void *p = map.lookup_read(
							keys[i].data(),
							[&](const void *v) {
								auto *q = (uint64_t *)v;
								k = std::atomic_ref<uint64_t>(q[0]).load(
									std::memory_order_acquire);
								a = std::atomic_ref<uint64_t>(q[1]).load(
									std::memory_order_acquire);
								b = std::atomic_ref<uint64_t>(q[2]).load(
									std::memory_order_acquire);
							});
						if (p == nullptr)
							miss++;
						else if (k != i || a != b)
							bad++;
					}
				}
				misses += miss;
				torn += bad;
				reads += n;
			});
		std::this_thread::sleep_for(std::chrono::seconds(2));
		stop = true;
		for (auto &t : threads)
			t.join();
		INFO("reads " << reads.load());
		CHECK(reads.load() > 0);
		CHECK(misses.load() == 0);
		CHECK(torn.load() == 0);
		CHECK(map.get_elem_count() == 2);
	}
}

TEST_CASE("bpftime_hash_map matches a reference map under churn",
	  "[bpftime_hash_map]")
{
	shared_memory_object::remove("bpftime_hash_map_churn_test");
	managed_shared_memory segment(create_only, "bpftime_hash_map_churn_test",
				      1 << 20);
	constexpr size_t capacity = 64;
	bpftime_hash_map map(segment, capacity, sizeof(uint32_t), sizeof(uint64_t));
	REQUIRE(map.bucket_count() == 64);
	std::map<uint32_t, uint64_t> reference;
	std::mt19937 rng(12345);
	// Keys collide often (few distinct values) and the table stays near
	// capacity, so deletes leave runs of deleted buckets behind.
	std::uniform_int_distribution<uint32_t> pick_key(0, 255);
	std::uniform_int_distribution<int> pick_op(0, 2);
	for (int step = 0; step < 50000; ++step) {
		uint32_t key = pick_key(rng);
		uint64_t value = step;
		switch (pick_op(rng)) {
		case 0: {
			const bool present = reference.count(key) != 0;
			const bool inserted = map.elem_update(&key, &value);
			REQUIRE(inserted == (present || reference.size() < capacity));
			if (inserted)
				reference[key] = value;
			break;
		}
		case 1:
			REQUIRE(map.elem_delete(&key) == (reference.erase(key) == 1));
			break;
		default: {
			auto *found = static_cast<uint64_t *>(map.elem_lookup(&key));
			auto it = reference.find(key);
			REQUIRE((found != nullptr) == (it != reference.end()));
			if (found)
				REQUIRE(*found == it->second);
		}
		}
		REQUIRE(map.get_elem_count() == reference.size());
	}
	for (uint32_t key = 0; key < 256; ++key) {
		auto *found = static_cast<uint64_t *>(map.elem_lookup(&key));
		REQUIRE((found != nullptr) == (reference.count(key) != 0));
	}
	shared_memory_object::remove("bpftime_hash_map_churn_test");
}

TEST_CASE("bpftime_hash_map keeps value pointers of live keys stable",
	  "[bpftime_hash_map]")
{
	shared_memory_object::remove("bpftime_hash_map_pointer_test");
	managed_shared_memory segment(create_only, "bpftime_hash_map_pointer_test",
				      1 << 20);
	bpftime_hash_map map(segment, 16, sizeof(uint32_t), sizeof(uint64_t));
	uint32_t kept = 7;
	uint64_t kept_value = 700;
	REQUIRE(map.elem_update(&kept, &kept_value));
	auto *pointer = map.elem_lookup(&kept);
	REQUIRE(pointer != nullptr);
	// Deleting other keys, including ones in the same probe run, must not
	// move the live key: BPF programs keep using returned value pointers.
	for (uint32_t round = 0; round < 200; ++round) {
		for (uint32_t i = 0; i < 15; ++i) {
			uint32_t key = 1000 + round * 15 + i;
			uint64_t value = key;
			REQUIRE(map.elem_update(&key, &value));
		}
		for (uint32_t i = 0; i < 15; ++i) {
			uint32_t key = 1000 + round * 15 + i;
			REQUIRE(map.elem_delete(&key));
		}
		REQUIRE(map.elem_lookup(&kept) == pointer);
		REQUIRE(*static_cast<uint64_t *>(pointer) == kept_value);
	}
	REQUIRE(map.get_elem_count() == 1);
	shared_memory_object::remove("bpftime_hash_map_pointer_test");
}

TEST_CASE("bpftime_hash_map keeps one bucket's long chain intact",
	  "[bpftime_hash_map]")
{
	shared_memory_object::remove("bpftime_hash_map_chain_test");
	managed_shared_memory segment(create_only, "bpftime_hash_map_chain_test",
				      1 << 20);
	bpftime_hash_map map(segment, 128, sizeof(uint32_t), sizeof(uint64_t));
	// 80 keys of one bucket: deletes at the head, in the middle and at the
	// tail of the chain leave the rest reachable.
	std::vector<uint32_t> keys;
	const uint32_t first = 1;
	for (uint32_t key = 1; keys.size() < 80 && key < (1u << 24); ++key)
		if (map.bucket_of_key(&key) == map.bucket_of_key(&first))
			keys.push_back(key);
	REQUIRE(keys.size() == 80);
	for (uint32_t key : keys) {
		uint64_t value = key;
		REQUIRE(map.elem_update(&key, &value));
	}
	REQUIRE(map.chain_length_of_key(&first) == 80);
	std::vector<bool> present(keys.size(), true);
	// The newest insert is the chain's head: delete it, the oldest (the
	// tail) and every third in between.
	for (size_t i : { size_t{ 79 }, size_t{ 0 } })
		present[i] = false;
	for (size_t i = 3; i < 79; i += 3)
		present[i] = false;
	for (size_t i = 0; i < keys.size(); i++)
		if (!present[i])
			REQUIRE(map.elem_delete(&keys[i]));
	size_t left = 0;
	for (size_t i = 0; i < keys.size(); i++) {
		void *v = map.elem_lookup(&keys[i]);
		if (present[i]) {
			left++;
			REQUIRE(v != nullptr);
			REQUIRE(*static_cast<uint64_t *>(v) == keys[i]);
		} else {
			REQUIRE(v == nullptr);
		}
	}
	REQUIRE(map.chain_length_of_key(&first) == left);
	REQUIRE(map.get_elem_count() == left);
	shared_memory_object::remove("bpftime_hash_map_chain_test");
}

// Churn near capacity (delete a key, insert a new one) locks only the
// stripes the two keys use. With element tokens parked per stripe, a map
// about two thirds full ran its shared pool dry and inserts locked every
// stripe to borrow one: 30 us per op at 90% full, 230 us with six threads.
TEST_CASE("bpftime_hash_map churn near capacity locks only the keys' stripes",
	  "[bpftime_hash_map][capacity]")
{
	shared_memory_object::remove("bpftime_hash_map_churn_test");
	managed_shared_memory segment(create_only, "bpftime_hash_map_churn_test",
				      16 << 20);
	const uint32_t max_entries = 10240, live = max_entries * 9 / 10;
	bpftime_hash_map map(segment, max_entries, 4, 8);
	REQUIRE(map.stripe_count() > 8);
	auto stripe_of = [&](uint32_t key) { return map.stripe_of_key(&key); };
	const uint64_t value = 1;
	uint32_t oldest = 0, next = 0;
	for (; next < live; next++)
		REQUIRE(map.elem_update(&next, &value));
	// FIFO churn spreads deletes and inserts over the stripes.
	for (int i = 0; i < 20000; i++, oldest++, next++) {
		REQUIRE(map.elem_delete(&oldest));
		REQUIRE(map.elem_update(&next, &value));
	}
	// Hold one stripe; churn on keys of other stripes must not wait for it.
	const size_t held = map.stripe_count() / 2;
	auto clear = [&](uint32_t key) { return stripe_of(key) != held; };
	std::vector<std::pair<uint32_t, uint32_t> > ops;
	for (; ops.size() < 2000; oldest++, next++)
		if (clear(oldest) && clear(next))
			ops.emplace_back(oldest, next);
		else {
			REQUIRE(map.elem_delete(&oldest));
			REQUIRE(map.elem_update(&next, &value));
		}
	map.lock_stripe_for_test(held);
	std::atomic<bool> done{ false };
	std::atomic<int> failed{ 0 };
	std::thread churn([&] {
		for (auto [old_key, new_key] : ops)
			if (!map.elem_delete(&old_key) ||
			    !map.elem_update(&new_key, &value))
				failed++;
		done = true;
	});
	for (int i = 0; i < 200 && !done; i++)
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	const bool waited = !done;
	map.unlock_stripe_for_test(held);
	churn.join();
	REQUIRE_FALSE(waited);
	REQUIRE(failed == 0);
	REQUIRE(map.get_elem_count() == live);
	shared_memory_object::remove("bpftime_hash_map_churn_test");
}

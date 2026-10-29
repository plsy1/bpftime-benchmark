// Hash map lookups take no lock while one writer deletes and re-inserts keys
// in the same bucket. The keys of each case have the same hash (the hasher is
// a base-31 polynomial, so byte pairs (1, 0) and (0, 31) contribute the same),
// so every insert reuses the bucket the last delete freed. A lookup that
// compared the key without re-checking the bucket's state word afterwards
// could match a key half overwritten by the next insert, or return a value
// not written yet.
#include <bpf_map/bpftime_hash_map.hpp>
#include <bpf_map/userspace/fix_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <linux/bpf.h>
#include <thread>
#include <vector>

using namespace boost::interprocess;
using namespace bpftime;

namespace
{
constexpr int readers = 3;
constexpr auto run_time = std::chrono::milliseconds(1500);

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

// 64-byte keys: halves A or B, then X or Y, all with the same hash. The
// writer alternates (A, X) and (B, Y); readers look up (A, Y), which is never
// inserted, so any hit is a match against a half-overwritten key.
using long_key = std::array<uint8_t, 64>;

long_key make_long_key(bool first_b, bool second_y)
{
	long_key key{};
	for (size_t i = 0; i < key.size(); i++)
		key[i] = (uint8_t)(i + 1);
	// Same contribution: (1, 0) and (0, 31) at a pair of positions.
	key[6] = first_b ? 0 : 1;
	key[7] = first_b ? 31 : 0;
	key[62] = second_y ? 0 : 1;
	key[63] = second_y ? 31 : 0;
	return key;
}
} // namespace

TEST_CASE("hash lookups never match a key being overwritten",
	  "[bpftime_hash_map][concurrency]")
{
	shm_guard shm("BPFTIME_TEST_HASH_LOCKFREE_TORN_KEY");
	bpftime_hash_map map(shm.segment, 16, sizeof(long_key), 8);
	const long_key ax = make_long_key(false, false);
	const long_key by = make_long_key(true, true);
	const long_key ay = make_long_key(false, true);
	REQUIRE(bpftime_hasher::hash_func(ax.data(), 64) ==
		bpftime_hasher::hash_func(by.data(), 64));
	REQUIRE(bpftime_hasher::hash_func(ay.data(), 64) ==
		bpftime_hasher::hash_func(ax.data(), 64));

	std::atomic<bool> done{ false };
	std::atomic<uint64_t> false_hits{ 0 }, lookups{ 0 };
	std::vector<std::thread> threads;
	for (int r = 0; r < readers; r++)
		threads.emplace_back([&] {
			uint64_t n = 0, bad = 0;
			while (!done.load(std::memory_order_relaxed)) {
				if (map.elem_lookup(ay.data()) != nullptr)
					bad++;
				n++;
			}
			false_hits += bad;
			lookups += n;
		});
	const uint64_t value = 1;
	const auto end = std::chrono::steady_clock::now() + run_time;
	uint64_t cycles = 0;
	while (std::chrono::steady_clock::now() < end) {
		for (int i = 0; i < 1000; i++) {
			REQUIRE(map.elem_update(ax.data(), &value));
			REQUIRE(map.elem_delete(ax.data()));
			REQUIRE(map.elem_update(by.data(), &value));
			REQUIRE(map.elem_delete(by.data()));
		}
		cycles += 1000;
	}
	done = true;
	for (auto &t : threads)
		t.join();
	INFO("writer cycles " << cycles << ", lookups " << lookups.load());
	REQUIRE(false_hits.load() == 0);
}

TEST_CASE("hash lookups return only values written for the key",
	  "[bpftime_hash_map][concurrency]")
{
	// Four-byte keys {0,0,1,0} and {0,0,0,31}; 256-byte values that hold
	// their key in the last four bytes (written last). The writer deletes
	// each key before inserting the other and counts each delete when it
	// starts and when it has finished. A reader that read the finished count,
	// found key K, read the value and then saw the same started count, ran
	// while no delete of K was in progress, so the value must hold K.
	shm_guard shm("BPFTIME_TEST_HASH_LOCKFREE_VALUE");
	struct value_t {
		uint8_t payload[252];
		uint32_t key;
	};
	bpftime_hash_map map(shm.segment, 16, 4, sizeof(value_t));
	const uint32_t keys[2] = { 0x00010000, 0x1f000000 };
	{
		uint8_t b0[4], b1[4];
		std::memcpy(b0, &keys[0], 4);
		std::memcpy(b1, &keys[1], 4);
		REQUIRE(bpftime_hasher::hash_func(b0, 4) ==
			bpftime_hasher::hash_func(b1, 4));
	}
	std::atomic<uint64_t> started[2] = { 0, 0 }, finished[2] = { 0, 0 };
	std::atomic<bool> done{ false };
	std::atomic<uint64_t> wrong{ 0 }, hits{ 0 };
	std::vector<std::thread> threads;
	for (int r = 0; r < readers; r++)
		threads.emplace_back([&, r] {
			const int k = r % 2;
			uint64_t bad = 0, n = 0;
			while (!done.load(std::memory_order_relaxed)) {
				const uint64_t before = finished[k].load();
				auto *v = static_cast<value_t *>(
					map.elem_lookup(&keys[k]));
				if (v == nullptr)
					continue;
				const uint32_t seen =
					std::atomic_ref<uint32_t>(v->key).load(
						std::memory_order_relaxed);
				std::atomic_thread_fence(
					std::memory_order_seq_cst);
				if (started[k].load() != before)
					continue; // Deleted meanwhile: may be reused.
				n++;
				if (seen != keys[k])
					bad++;
			}
			wrong += bad;
			hits += n;
		});
	value_t values[2];
	for (int k = 0; k < 2; k++) {
		std::memset(values[k].payload, k + 1, sizeof(values[k].payload));
		values[k].key = keys[k];
	}
	const auto end = std::chrono::steady_clock::now() + run_time;
	while (std::chrono::steady_clock::now() < end) {
		for (int i = 0; i < 1000; i++) {
			for (int k = 0; k < 2; k++) {
				REQUIRE(map.elem_update(&keys[k], &values[k]));
				started[k].fetch_add(1);
				REQUIRE(map.elem_delete(&keys[k]));
				finished[k].fetch_add(1);
			}
		}
	}
	done = true;
	for (auto &t : threads)
		t.join();
	INFO("checked hits " << hits.load());
	REQUIRE(hits.load() > 0);
	REQUIRE(wrong.load() == 0);
}

TEST_CASE("fixed-size hash map locks its own writers",
	  "[bpftime_hash_map][concurrency]")
{
	// Callers no longer take the map lock (should_lock is false): updates,
	// deletes and get_next_key lock inside the implementation, lookups not.
	shm_guard shm("BPFTIME_TEST_HASH_LOCKFREE_NO_WAIT");
	fix_size_hash_map_impl map(shm.segment, 1024, 4, 8);
	REQUIRE_FALSE(fix_size_hash_map_impl::should_lock);
	for (uint32_t k = 0; k < 512; k++) {
		const uint64_t v = k;
		REQUIRE(map.elem_update(&k, &v, BPF_ANY) == 0);
	}
	std::atomic<bool> done{ false };
	std::thread writer([&] {
		uint32_t k = 1000;
		const uint64_t v = 0;
		while (!done.load(std::memory_order_relaxed)) {
			map.elem_update(&k, &v, BPF_ANY);
			map.elem_delete(&k);
		}
	});
	for (int round = 0; round < 200000; round++) {
		const uint32_t k = round % 512;
		const auto *v =
			static_cast<const uint64_t *>(map.elem_lookup(&k));
		REQUIRE(v != nullptr);
		REQUIRE(*v == k);
	}
	done = true;
	writer.join();
	uint32_t next;
	uint32_t count = 0;
	const void *prev = nullptr;
	uint32_t cur;
	while (map.map_get_next_key(prev, &next) == 0) {
		cur = next;
		prev = &cur;
		count++;
	}
	REQUIRE(count == 512);
}

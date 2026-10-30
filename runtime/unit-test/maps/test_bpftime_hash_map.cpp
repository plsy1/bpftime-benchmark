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
#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
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

	SECTION("Delete preserves a colliding probe chain")
	{
		// With the 23-bucket table used by this fixture (twice the
		// capacity, next prime), these integer keys hash to the same initial
		// bucket on little-endian platforms.
		REQUIRE(map.bucket_count() == 23);
		int key1 = 1;
		int key2 = 24;
		int key3 = 47;
		int64_t value1 = 101;
		int64_t value2 = 112;
		int64_t value3 = 123;

		REQUIRE(map.elem_update(&key1, &value1) == true);
		REQUIRE(map.elem_update(&key2, &value2) == true);
		REQUIRE(map.elem_delete(&key1) == true);

		auto *second = static_cast<int64_t *>(map.elem_lookup(&key2));
		REQUIRE(second != nullptr);
		REQUIRE(*second == value2);

		// A later insertion may reuse the tombstone without making the
		// existing colliding key unreachable.
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

TEST_CASE("bpftime_hash_map matches a reference map under churn",
	  "[bpftime_hash_map]")
{
	shared_memory_object::remove("bpftime_hash_map_churn_test");
	managed_shared_memory segment(create_only, "bpftime_hash_map_churn_test",
				      1 << 20);
	constexpr size_t capacity = 64;
	bpftime_hash_map map(segment, capacity, sizeof(uint32_t), sizeof(uint64_t));
	REQUIRE(map.bucket_count() == 131);
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

TEST_CASE("bpftime_hash_map probe bound shrinks after far elements are deleted",
	  "[bpftime_hash_map]")
{
	shared_memory_object::remove("bpftime_hash_map_bound_test");
	managed_shared_memory segment(create_only, "bpftime_hash_map_bound_test",
				      1 << 20);
	bpftime_hash_map map(segment, 128, sizeof(uint32_t), sizeof(uint64_t));
	// Keys sharing one home bucket sit at displacements 0, 1, 2, ...
	std::vector<uint32_t> keys;
	uint32_t first = 1;
	const size_t target = bpftime_hasher::hash_func(&first, sizeof(first)) %
			      map.bucket_count();
	for (uint32_t key = 1; keys.size() < 80 && key < (1u << 24); ++key)
		if (bpftime_hasher::hash_func(&key, sizeof(key)) % map.bucket_count() ==
		    target)
			keys.push_back(key);
	REQUIRE(keys.size() == 80);
	for (uint32_t key : keys) {
		uint64_t value = key;
		REQUIRE(map.elem_update(&key, &value));
	}
	REQUIRE(map.max_displacement() == 79);
	// While an element beyond the tracked range remains, the bound stays
	// conservative; it becomes exact once all of them are gone.
	const size_t tracked = bpftime_hash_map::tracked_displacements();
	REQUIRE(tracked < 79);
	for (size_t i = 79; i > tracked; --i)
		REQUIRE(map.elem_delete(&keys[i]));
	REQUIRE(map.max_displacement() == 79);
	REQUIRE(map.elem_delete(&keys[tracked]));
	REQUIRE(map.max_displacement() == tracked - 1);
	for (size_t i = tracked - 1; i >= 1; --i)
		REQUIRE(map.elem_delete(&keys[i]));
	REQUIRE(map.max_displacement() == 0);
	REQUIRE(*static_cast<uint64_t *>(map.elem_lookup(&keys[0])) == keys[0]);
	for (size_t i = 1; i < keys.size(); ++i)
		REQUIRE(map.elem_lookup(&keys[i]) == nullptr);
	// A deleted middle element keeps the bound for the element after it.
	uint64_t one = 1, two = 2;
	REQUIRE(map.elem_update(&keys[1], &one));
	REQUIRE(map.elem_update(&keys[2], &two));
	REQUIRE(map.max_displacement() == 2);
	REQUIRE(map.elem_delete(&keys[1]));
	REQUIRE(map.max_displacement() == 2);
	REQUIRE(*static_cast<uint64_t *>(map.elem_lookup(&keys[2])) == 2);
	shared_memory_object::remove("bpftime_hash_map_bound_test");
}

// A stripe with no tokens left, once the shared pool was empty, took one
// token from another stripe per insert, locking every stripe each time; it
// now takes a batch.
TEST_CASE("bpftime_hash_map moves tokens to a stripe in batches",
	  "[bpftime_hash_map][tokens]")
{
	shared_memory_object::remove("bpftime_hash_map_tokens_test");
	managed_shared_memory segment(create_only, "bpftime_hash_map_tokens_test",
				      1 << 20);
	bpftime_hash_map map(segment, 128, 4, 8);
	REQUIRE(map.stripe_count() > 4);
	auto stripe_of = [&](uint32_t key) {
		return bpftime_hasher::hash_func(&key, 4) % map.bucket_count() /
		       map.buckets_per_stripe();
	};
	// Drain the pool into the first stripes that see an insert.
	std::vector<bool> used(map.stripe_count());
	uint64_t value = 1;
	uint32_t key = 1;
	for (; map.pool_tokens() > 0; key++) {
		used[stripe_of(key)] = true;
		REQUIRE(map.elem_update(&key, &value));
	}
	// A key homing in a stripe that has no tokens.
	for (;; key++) {
		const size_t s = stripe_of(key);
		if (used[s] || map.stripe_tokens(s) != 0)
			continue;
		REQUIRE(map.elem_update(&key, &value));
		CHECK(map.stripe_tokens(s) > 1);
		break;
	}
	shared_memory_object::remove("bpftime_hash_map_tokens_test");
}

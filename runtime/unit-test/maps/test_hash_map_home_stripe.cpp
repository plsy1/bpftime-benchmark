// Updates and deletes whose buckets all lie in the home stripe take only
// that stripe's lock (the fast path); any other bucket they read or write
// means the general path, which locks its stripe too. Another thread holds a
// stripe's lock, and each operation must wait for it exactly when it touches
// that stripe: a fast path reaching past its stripe would write a bucket
// whose stripe another writer may hold.
#include <bpf_map/bpftime_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

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

size_t home_of(const bpftime_hash_map &map, uint32_t key)
{
	return bpftime_hasher::hash_func(&key, sizeof(key)) % map.bucket_count();
}

// count keys not handed out before (from next on) whose home bucket is home.
std::vector<uint32_t> keys_at(const bpftime_hash_map &map, size_t home,
			      size_t count, uint32_t &next)
{
	std::vector<uint32_t> keys;
	for (; keys.size() < count; next++) {
		if (home_of(map, next) == home)
			keys.push_back(next);
	}
	return keys;
}

// Runs op in another thread while this one holds stripe's lock. Returns
// whether op waited for the lock (had not finished before it was released);
// op's own result goes to ok (checked here, not in the other thread).
template <class Op>
bool waits_for_stripe(bpftime_hash_map &map, size_t stripe, Op op)
{
	map.lock_stripe_for_test(stripe);
	std::atomic<bool> done{ false };
	bool ok = false;
	std::thread runner([&] {
		ok = op();
		done.store(true);
	});
	// A fast path finishes at once; give it time anyway.
	for (int i = 0; i < 100 && !done.load(); i++)
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	const bool waited = !done.load();
	map.unlock_stripe_for_test(stripe);
	runner.join();
	REQUIRE(ok);
	return waited;
}

size_t index_of(bpftime_hash_map &map, uint32_t key)
{
	void *value = map.elem_lookup(&key);
	REQUIRE(value != nullptr);
	return map.get_index_of_value(value);
}
} // namespace

TEST_CASE("hash map writers lock every stripe they touch beyond the home stripe",
	  "[bpftime_hash_map][home-stripe]")
{
	shm_guard shm("bpftime_hash_home_stripe_test");
	bpftime_hash_map map(shm.segment, 1000, sizeof(uint32_t),
			     sizeof(uint64_t));
	const size_t per_stripe = map.buckets_per_stripe();
	REQUIRE(per_stripe == 64);
	REQUIRE(map.bucket_count() % per_stripe != 0); // last stripe is partial
	const size_t s = 1, end = (s + 1) * per_stripe;
	const size_t home = end - 8;
	uint32_t next = 1;
	const uint64_t value = 42, other = 7;
	// Whether each operation waited for a lock held on stripe held.
	auto update_waits = [&](size_t held, uint32_t key, uint64_t v) {
		return waits_for_stripe(map, held,
					[&] { return map.elem_update(&key, &v); });
	};
	auto delete_waits = [&](size_t held, uint32_t key) {
		return waits_for_stripe(map, held,
					[&] { return map.elem_delete(&key); });
	};

	// Seven keys at home fill home .. end - 2, within the stripe.
	for (uint32_t key : keys_at(map, home, 7, next))
		REQUIRE_FALSE(update_waits(s + 1, key, value));

	// The first free bucket is the stripe's last: the fast path takes it.
	const uint32_t at_last = keys_at(map, home, 1, next)[0];
	REQUIRE_FALSE(update_waits(s + 1, at_last, value));
	REQUIRE(index_of(map, at_last) == end - 1);

	// Now it is the next stripe's first: that stripe must be locked.
	const uint32_t past_end = keys_at(map, home, 1, next)[0];
	REQUIRE(update_waits(s + 1, past_end, value));
	REQUIRE(index_of(map, past_end) == end);

	// A present key homed early in the stripe: its probe bound stays inside.
	const uint32_t inner_key =
		keys_at(map, s * per_stripe + 4, 1, next)[0];
	REQUIRE_FALSE(update_waits(s + 1, inner_key, value));

	// A key homed where the bound reaches the next stripe: the update has
	// to see that stripe unchanged.
	REQUIRE(update_waits(s + 1, at_last, other));
	REQUIRE(*static_cast<uint64_t *>(map.elem_lookup(&at_last)) == other);

	// Delete reads the bucket after the bound: here the next stripe's.
	REQUIRE(delete_waits(s + 1, at_last));
	REQUIRE(map.elem_lookup(&at_last) == nullptr);
	// A delete whose bound and next bucket are in its stripe does not.
	REQUIRE_FALSE(delete_waits(s + 1, inner_key));

	// A key alone in its stripe's last bucket, the next stripe empty: the
	// delete locks the next stripe to find its first bucket empty, and turns
	// its own bucket empty instead of leaving a tombstone.
	const size_t s3_last = 4 * per_stripe - 1;
	const uint32_t lone = keys_at(map, s3_last, 1, next)[0];
	REQUIRE_FALSE(update_waits(4, lone, value));
	REQUIRE(index_of(map, lone) == s3_last);
	REQUIRE(delete_waits(4, lone));
	REQUIRE(map.get_bucket_state(s3_last) == 0); // empty, not deleted

	// The last, partial stripe: a run reaching the table's end wraps to
	// bucket 0 (stripe 0), which must be locked.
	const size_t last_bucket = map.bucket_count() - 1;
	REQUIRE(map.stripe_count() - 1 > 4);
	const auto wrapping = keys_at(map, last_bucket, 2, next);
	REQUIRE_FALSE(update_waits(0, wrapping[0], value));
	REQUIRE(index_of(map, wrapping[0]) == last_bucket);
	REQUIRE(update_waits(0, wrapping[1], value));
	REQUIRE(index_of(map, wrapping[1]) == 0);

	// Every key still reads back after the mix of paths.
	for (uint32_t key : { past_end, wrapping[0], wrapping[1] })
		REQUIRE(*static_cast<uint64_t *>(map.elem_lookup(&key)) ==
			value);
}

// Two ways to use a hash map value beyond returning its pointer, for maps
// whose value refers to more storage (the per-CPU hash keeps a value slot
// number there):
// - lookup_read reads the value inside the lookup's check, so a lookup that
//   succeeds read its own key's value, never that of a key that took the
//   bucket over meanwhile;
// - with_existing_value works on the value under the locks of every stripe
//   the key's probe covers, as a writer, so no delete or insert changes the
//   bucket meanwhile.
#include <bpf_map/bpftime_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
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

// Whether op waited for stripe's lock held by this thread.
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
	for (int i = 0; i < 100 && !done.load(); i++)
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	const bool waited = !done.load();
	map.unlock_stripe_for_test(stripe);
	runner.join();
	REQUIRE(ok);
	return waited;
}

uint64_t load_value(const void *value)
{
	return std::atomic_ref<uint64_t>(
		       *static_cast<uint64_t *>(const_cast<void *>(value)))
		.load(std::memory_order_acquire);
}
} // namespace

TEST_CASE("hash lookup_read reads the value of the key it matched",
	  "[bpftime_hash_map][value-access]")
{
	shm_guard shm("bpftime_hash_lookup_read_test");
	bpftime_hash_map map(shm.segment, 16, sizeof(uint32_t),
			     sizeof(uint64_t));
	// Same home bucket ({0,0,1,0} and {0,0,0,31}).
	const uint32_t first = 0x00010000, second = 0x1f000000;
	REQUIRE(home_of(map, first) == home_of(map, second));
	const uint64_t first_value = 100, second_value = 200;
	REQUIRE(map.elem_update(&first, &first_value));
	const size_t bucket = map.get_index_of_value(map.elem_lookup(&first));

	// The reader stops in its first read until the bucket holds the other
	// key. The lookup must then retry (and miss), not return what it read.
	std::atomic<bool> reading{ false }, replaced{ false };
	uint64_t read_value = 0;
	void *found = nullptr;
	std::thread reader([&] {
		bool first_read = true;
		found = map.lookup_read(&first, [&](const void *value) {
			if (first_read) {
				first_read = false;
				reading.store(true);
				while (!replaced.load())
					std::this_thread::yield();
			}
			read_value = load_value(value);
		});
	});
	while (!reading.load())
		std::this_thread::yield();
	REQUIRE(map.elem_delete(&first));
	REQUIRE(map.elem_update(&second, &second_value));
	REQUIRE(map.get_index_of_value(map.elem_lookup(&second)) == bucket);
	replaced.store(true);
	reader.join();
	INFO("read " << read_value);
	REQUIRE(found == nullptr);

	// Without a writer: the key's own value.
	REQUIRE(map.lookup_read(&second, [&](const void *value) {
		read_value = load_value(value);
	}) != nullptr);
	REQUIRE(read_value == second_value);
}

TEST_CASE("hash with_existing_value locks every stripe the key's probe covers",
	  "[bpftime_hash_map][value-access]")
{
	shm_guard shm("bpftime_hash_with_existing_test");
	bpftime_hash_map map(shm.segment, 1000, sizeof(uint32_t),
			     sizeof(uint64_t));
	const size_t per_stripe = map.buckets_per_stripe();
	REQUIRE(per_stripe == 64);
	const size_t s = 1, end = (s + 1) * per_stripe;
	uint32_t next = 1;
	const uint64_t value = 42, changed = 43;
	// Nine keys homed at end - 8: the last one lies in the next stripe, and
	// the stripe's probe bound (8) reaches it.
	const auto run = keys_at(map, end - 8, 9, next);
	for (uint32_t key : run)
		REQUIRE(map.elem_update(&key, &value));
	const uint32_t past_end = run.back();
	REQUIRE(map.get_index_of_value(map.elem_lookup(&past_end)) == end);
	// A key homed early in the stripe: its bound stays inside.
	const uint32_t inner = keys_at(map, s * per_stripe + 4, 1, next)[0];
	REQUIRE(map.elem_update(&inner, &value));

	auto set = [&](uint32_t key) {
		return [&map, key, &changed] {
			return map.with_existing_value(&key, [&](void *v) {
				std::memcpy(v, &changed, sizeof(changed));
			});
		};
	};
	// Every key waits for its home stripe.
	REQUIRE(waits_for_stripe(map, s, set(inner)));
	REQUIRE(waits_for_stripe(map, s, set(run[0])));
	// A probe reaching the next stripe waits for it too ...
	REQUIRE(waits_for_stripe(map, s + 1, set(run[0])));
	REQUIRE(waits_for_stripe(map, s + 1, set(past_end)));
	// ... one inside its stripe does not.
	REQUIRE_FALSE(waits_for_stripe(map, s + 1, set(inner)));
	for (uint32_t key : { inner, run[0], past_end })
		REQUIRE(load_value(map.elem_lookup(&key)) == changed);

	// A missing key: fn does not run.
	const uint32_t missing = keys_at(map, end - 8, 1, next)[0];
	bool ran = false;
	REQUIRE_FALSE(map.with_existing_value(&missing,
					      [&](void *) { ran = true; }));
	REQUIRE_FALSE(ran);
}

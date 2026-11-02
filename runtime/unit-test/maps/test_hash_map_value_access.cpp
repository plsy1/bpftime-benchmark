// Two ways to use a hash map value beyond returning its pointer, for maps
// whose value refers to more storage (the per-CPU hash keeps a value slot
// number there):
// - lookup_read reads the value inside the lookup's check, so a lookup that
//   succeeds read its own key's value, never that of a key that took the
//   element over meanwhile;
// - with_existing_value works on the value under the key's stripe lock, as a
//   writer, so no delete frees the element meanwhile.
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
	// The second key in another bucket: the element moves to another chain.
	const uint32_t first = 0x00010000;
	uint32_t second = first + 1;
	while (map.bucket_of_key(&second) == map.bucket_of_key(&first))
		second++;
	const uint64_t first_value = 100, second_value = 200;
	REQUIRE(map.elem_update(&first, &first_value));
	const size_t element = map.get_index_of_value(map.elem_lookup(&first));

	// The reader stops in its first read until the element holds the other
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
	REQUIRE(map.get_index_of_value(map.elem_lookup(&second)) == element);
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

TEST_CASE("hash with_existing_value locks the key's stripe",
	  "[bpftime_hash_map][value-access]")
{
	shm_guard shm("bpftime_hash_with_existing_test");
	bpftime_hash_map map(shm.segment, 1000, sizeof(uint32_t),
			     sizeof(uint64_t));
	REQUIRE(map.stripe_count() > 2);
	const uint64_t value = 42, changed = 43;
	const uint32_t key = 7;
	REQUIRE(map.elem_update(&key, &value));
	const size_t s = map.stripe_of_key(&key);
	auto set = [&] {
		return map.with_existing_value(&key, [&](void *v) {
			std::memcpy(v, &changed, sizeof(changed));
		});
	};
	REQUIRE(waits_for_stripe(map, s, set));
	REQUIRE_FALSE(waits_for_stripe(map, (s + 1) % map.stripe_count(), set));
	REQUIRE(load_value(map.elem_lookup(&key)) == changed);

	// A missing key: fn does not run.
	const uint32_t missing = 8;
	bool ran = false;
	REQUIRE_FALSE(map.with_existing_value(&missing,
					      [&](void *) { ran = true; }));
	REQUIRE_FALSE(ran);
}

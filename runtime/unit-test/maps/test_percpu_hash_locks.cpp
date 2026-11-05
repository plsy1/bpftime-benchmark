// The per-CPU hash keeps its keys in the striped hash table and its values in
// a slot pool. Every operation used to run under the handler's map lock (the
// Boost index it had rehashed), so threads on different CPUs serialized even
// on lookups. Now:
// - nothing takes the handler's map lock;
// - lookups and updates of an existing key take no pool lock (only inserts
//   of a new key and deletes do);
// - an update of an existing key writes under the key's stripe lock, so a
//   concurrent delete cannot give its slot to another key meanwhile.
#include "handler/map_handler.hpp"
#include <bpf_map/userspace/per_cpu_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <linux/bpf.h>
#include <string>
#include <sched.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include "platform_utils.hpp"

using namespace boost::interprocess;
using namespace bpftime;

namespace
{
struct shm_guard {
	std::string name;
	managed_shared_memory segment;
	explicit shm_guard(const std::string &name)
		: name((shared_memory_object::remove(name.c_str()), name)),
		  segment(create_only, name.c_str(), 16 << 20)
	{
	}
	~shm_guard()
	{
		shared_memory_object::remove(name.c_str());
	}
};

// Runs op in another thread while lock(), and says whether op had to wait
// for unlock() (had not finished after a while).
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

size_t stripe_of_key(per_cpu_hash_map_impl &map, uint32_t key)
{
	return map.table_for_test().stripe_of_key(&key);
}
} // namespace

TEST_CASE("per-cpu hash lookups and updates take no pool lock",
	  "[percpu-hash][percpu-locks]")
{
	shm_guard shm("bpftime_percpu_locks_pool_" + std::to_string(getpid()));
	per_cpu_hash_map_impl map(shm.segment, 4, 8, 64);
	const uint32_t key = 7, other = 8;
	const uint64_t one = 1, two = 2;
	REQUIRE(map.elem_update(&key, &one, BPF_ANY) == 0);
	auto lock = [&] { map.lock_pool_for_test(); };
	auto unlock = [&] { map.unlock_pool_for_test(); };

	const void *found = nullptr;
	REQUIRE_FALSE(waits(lock, unlock, [&] { found = map.elem_lookup(&key); }));
	REQUIRE(found != nullptr);
	long ret = -1;
	REQUIRE_FALSE(waits(lock, unlock, [&] {
		ret = map.elem_update(&key, &two, BPF_ANY);
	}));
	REQUIRE(ret == 0);
	REQUIRE_FALSE(waits(lock, unlock, [&] {
		ret = map.elem_update(&key, &two, BPF_EXIST);
	}));
	REQUIRE(ret == 0);
	REQUIRE_FALSE(waits(lock, unlock, [&] {
		found = map.elem_lookup_userspace(&key);
	}));
	REQUIRE(found != nullptr);
	// A new key needs a slot.
	REQUIRE(waits(lock, unlock, [&] {
		ret = map.elem_update(&other, &one, BPF_NOEXIST);
	}));
	REQUIRE(ret == 0);
}

// BPF_NOEXIST on an existing key used to take a slot (the pool lock) before
// finding the key (review 2026-11-05), the losing side of every
// lookup_or_try_init race.
TEST_CASE("per-cpu hash BPF_NOEXIST on an existing key takes no slot",
	  "[percpu-hash][percpu-locks]")
{
	shm_guard shm("bpftime_percpu_locks_noexist_" + std::to_string(getpid()));
	SECTION("No pool lock")
	{
		per_cpu_hash_map_impl map(shm.segment, 4, 8, 64);
		const uint32_t key = 7;
		const uint64_t one = 1;
		REQUIRE(map.elem_update(&key, &one, BPF_ANY) == 0);
		std::vector<uint64_t> all(bpftime_num_possible_cpus(), 2);
		long ret = 0;
		int err = 0;
		REQUIRE_FALSE(waits([&] { map.lock_pool_for_test(); },
				    [&] { map.unlock_pool_for_test(); }, [&] {
					    ret = map.elem_update(&key, &one,
								  BPF_NOEXIST);
					    err = errno;
				    }));
		CHECK(ret == -1);
		CHECK(err == EEXIST);
		REQUIRE_FALSE(waits([&] { map.lock_pool_for_test(); },
				    [&] { map.unlock_pool_for_test(); }, [&] {
					    ret = map.elem_update_userspace(
						    &key, all.data(),
						    BPF_NOEXIST);
					    err = errno;
				    }));
		CHECK(ret == -1);
		CHECK(err == EEXIST);
	}
	SECTION("EEXIST, not E2BIG, while an insert holds the spare slot")
	{
		// One CPU: max_entries + 1 slots. Two keys fill the map; an
		// insert of a third takes the spare slot and waits for its
		// stripe meanwhile.
		cpu_set_t old_set, set;
		REQUIRE(sched_getaffinity(0, sizeof(old_set), &old_set) == 0);
		CPU_ZERO(&set);
		CPU_SET(0, &set);
		REQUIRE(sched_setaffinity(0, sizeof(set), &set) == 0);
		per_cpu_hash_map_impl map(shm.segment, 4, 8, 2, 1);
		const uint32_t k1 = 11, k2 = 22;
		const uint64_t v = 1;
		REQUIRE(map.elem_update(&k1, &v, BPF_ANY) == 0);
		REQUIRE(map.elem_update(&k2, &v, BPF_ANY) == 0);
		// A map this small has one stripe: k2's too is locked, which
		// the lock-free lookup of BPF_NOEXIST never waits for.
		const uint32_t k3 = 33;
		auto &table = map.table_for_test();
		const size_t stripe = table.stripe_of_key(&k3);
		table.lock_stripe_for_test(stripe);
		long ret_insert = 0;
		std::thread insert([&] {
			cpu_set_t one_cpu;
			CPU_ZERO(&one_cpu);
			CPU_SET(0, &one_cpu);
			sched_setaffinity(0, sizeof(one_cpu), &one_cpu);
			// NOEXIST: no stripe lock before the slot.
			ret_insert = map.elem_update(&k3, &v, BPF_NOEXIST);
		});
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		errno = 0;
		const long ret = map.elem_update(&k2, &v, BPF_NOEXIST);
		const int err = errno;
		table.unlock_stripe_for_test(stripe);
		insert.join();
		sched_setaffinity(0, sizeof(old_set), &old_set);
		CHECK(ret == -1);
		CHECK(err == EEXIST);
		CHECK(ret_insert == -1); // the map is full
	}
}

TEST_CASE("per-cpu hash updates of an existing key wait for its stripe",
	  "[percpu-hash][percpu-locks]")
{
	shm_guard shm("bpftime_percpu_locks_stripe_" + std::to_string(getpid()));
	per_cpu_hash_map_impl map(shm.segment, 4, 8, 1000);
	auto &table = map.table_for_test();
	const uint32_t key = 7;
	const uint64_t one = 1, two = 2;
	REQUIRE(map.elem_update(&key, &one, BPF_ANY) == 0);
	const size_t stripe = stripe_of_key(map, key);
	const size_t elsewhere = (stripe + 2) % table.stripe_count();
	long ret = -1;
	auto update = [&] { ret = map.elem_update(&key, &two, BPF_ANY); };
	REQUIRE(waits([&] { table.lock_stripe_for_test(stripe); },
		      [&] { table.unlock_stripe_for_test(stripe); }, update));
	REQUIRE(ret == 0);
	REQUIRE_FALSE(waits([&] { table.lock_stripe_for_test(elsewhere); },
			    [&] { table.unlock_stripe_for_test(elsewhere); },
			    update));
	// bpf() updates too.
	std::vector<uint64_t> values(bpftime_num_possible_cpus(), 3);
	REQUIRE(waits([&] { table.lock_stripe_for_test(stripe); },
		      [&] { table.unlock_stripe_for_test(stripe); },
		      [&] {
			      ret = map.elem_update_userspace(&key, values.data(),
							      BPF_EXIST);
		      }));
	REQUIRE(ret == 0);
	// Lookups do not wait for writers.
	const void *found = nullptr;
	REQUIRE_FALSE(waits([&] { table.lock_stripe_for_test(stripe); },
			    [&] { table.unlock_stripe_for_test(stripe); },
			    [&] { found = map.elem_lookup(&key); }));
	REQUIRE(found != nullptr);
}

TEST_CASE("per-cpu hash operations do not take the handler's map lock",
	  "[percpu-hash][percpu-locks]")
{
	shm_guard shm("bpftime_percpu_locks_handler_" + std::to_string(getpid()));
	bpf_map_handler map(1, BPF_MAP_TYPE_PERCPU_HASH, 4, 8, 64, 0,
			    "pcpu_hash_locks", shm.segment);
	REQUIRE(map.map_init(shm.segment) == 0);
	auto lock = [&] { pthread_spin_lock(&map.get_raw_spin_lock()); };
	auto unlock = [&] { pthread_spin_unlock(&map.get_raw_spin_lock()); };
	const uint32_t key = 7;
	const uint64_t one = 1;
	std::vector<uint64_t> out(bpftime_num_possible_cpus());
	long ret = -1;
	const void *found = nullptr;
	REQUIRE_FALSE(waits(lock, unlock, [&] {
		ret = map.map_update_percpu_hash(&key, &one, BPF_ANY);
	}));
	REQUIRE(ret == 0);
	REQUIRE_FALSE(waits(lock, unlock,
			    [&] { found = map.map_lookup_percpu_hash(&key); }));
	REQUIRE(found != nullptr);
	REQUIRE_FALSE(waits(lock, unlock, [&] {
		ret = map.map_update_elem(&key, &one, BPF_EXIST, false);
	}));
	REQUIRE(ret == 0);
	REQUIRE_FALSE(waits(lock, unlock,
			    [&] { found = map.map_lookup_elem(&key, false); }));
	REQUIRE(found != nullptr);
	REQUIRE_FALSE(waits(lock, unlock, [&] {
		ret = map.map_lookup_and_delete_elem(&key, out.data());
	}));
	REQUIRE(ret == 0);
	REQUIRE(map.map_update_percpu_hash(&key, &one, BPF_ANY) == 0);
	REQUIRE_FALSE(waits(lock, unlock,
			    [&] { ret = map.map_delete_elem(&key, false); }));
	REQUIRE(ret == 0);
	map.map_free(shm.segment);
}

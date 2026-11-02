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
	auto &table = map.table_for_test();
	return bpftime_hasher::hash_func(&key, sizeof(key)) %
	       table.bucket_count() / table.buckets_per_stripe();
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

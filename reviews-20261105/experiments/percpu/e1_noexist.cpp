// E1: BPF_NOEXIST on an existing key.
// (a) It takes the pool lock (CLAUDE.md: the pool lock is "taken only to
//     insert a new key or to delete one").
// (b) With the slot pool exhausted by in-flight inserts it fails with E2BIG
//     where the kernel reports EEXIST (the key exists; no element is needed).
#include <bpf_map/userspace/per_cpu_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <linux/bpf.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <unistd.h>

using namespace boost::interprocess;
using namespace bpftime;

template <class Lock, class Unlock, class Op>
static bool waits(Lock lock, Unlock unlock, Op op)
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

int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);
	const std::string name = "e1_noexist_" + std::to_string(getpid());
	shared_memory_object::remove(name.c_str());
	int failures = 0;
	{
		managed_shared_memory seg(create_only, name.c_str(), 8 << 20);
		// (a) the pool lock
		{
			per_cpu_hash_map_impl map(seg, 4, 8, 64);
			const uint32_t key = 7;
			const uint64_t one = 1, two = 2;
			if (map.elem_update(&key, &one, BPF_ANY) != 0)
				return 2;
			long ret = 0;
			int err = 0;
			const bool w = waits(
				[&] { map.lock_pool_for_test(); },
				[&] { map.unlock_pool_for_test(); },
				[&] {
					ret = map.elem_update(&key, &two,
							      BPF_NOEXIST);
					err = errno;
				});
			printf("(a) update(existing key, BPF_NOEXIST) while the pool lock is held: %s (ret %ld errno %d %s)\n",
			       w ? "WAITED for the pool lock" : "did not wait",
			       ret, err, strerror(err));
			if (w)
				failures++;
			// bpf() path too
			std::vector<uint64_t> vals(bpftime_num_possible_cpus(), 5);
			const bool w2 = waits(
				[&] { map.lock_pool_for_test(); },
				[&] { map.unlock_pool_for_test(); },
				[&] {
					ret = map.elem_update_userspace(
						&key, vals.data(), BPF_NOEXIST);
					err = errno;
				});
			printf("(a) bpf() update(existing key, BPF_NOEXIST) while the pool lock is held: %s (ret %ld errno %d %s)\n",
			       w2 ? "WAITED for the pool lock" : "did not wait",
			       ret, err, strerror(err));
			if (w2)
				failures++;
		}
		// (b) E2BIG instead of EEXIST: ncpu = 1 so the pool has
		// max_entries + 1 slots; two keys fill the table; one in-flight
		// NOEXIST update of an existing key holds the spare slot while
		// it waits for the stripe lock; a second one gets E2BIG.
		{
			per_cpu_hash_map_impl map(seg, 4, 8, 2, /*ncpu*/ 1);
			auto &table = map.table_for_test();
			const uint32_t k1 = 11, k2 = 22;
			const uint64_t v = 1;
			// Program path: pin to CPU 0, the only CPU the map has.
			cpu_set_t set;
			CPU_ZERO(&set);
			CPU_SET(0, &set);
			if (sched_setaffinity(0, sizeof(set), &set) != 0) {
				perror("sched_setaffinity");
				return 3;
			}
			std::vector<uint64_t> all(bpftime_num_possible_cpus(), 9);
			if (map.elem_update_userspace(&k1, all.data(), BPF_ANY) != 0 ||
			    map.elem_update_userspace(&k2, all.data(), BPF_ANY) != 0) {
				perror("fill");
				return 4;
			}
			const size_t stripe = table.stripe_of_key(&k1);
			table.lock_stripe_for_test(stripe);
			std::atomic<bool> done{ false };
			long ret_a = 0;
			int err_a = 0;
			std::thread a([&] {
				ret_a = map.elem_update(&k1, &v, BPF_NOEXIST);
				err_a = errno;
				done.store(true);
			});
			// Let thread A take the spare slot and block on the stripe.
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			errno = 0;
			long ret_b = map.elem_update(&k2, &v, BPF_NOEXIST);
			int err_b = errno;
			errno = 0;
			long ret_c = map.elem_update_userspace(&k2, all.data(),
							       BPF_NOEXIST);
			int err_c = errno;
			table.unlock_stripe_for_test(stripe);
			a.join();
			printf("(b) in-flight NOEXIST update of existing k1 (blocked on the stripe lock): ret %ld errno %d %s\n",
			       ret_a, err_a, strerror(err_a));
			printf("(b) meanwhile program update(existing k2, BPF_NOEXIST): ret %ld errno %d %s (kernel: EEXIST)\n",
			       ret_b, err_b, strerror(err_b));
			printf("(b) meanwhile bpf() update(existing k2, BPF_NOEXIST): ret %ld errno %d %s (kernel: EEXIST)\n",
			       ret_c, err_c, strerror(err_c));
			if (err_b != EEXIST || err_c != EEXIST)
				failures++;
		}
	}
	shared_memory_object::remove(name.c_str());
	printf("failures: %d\n", failures);
	return failures ? 1 : 0;
}

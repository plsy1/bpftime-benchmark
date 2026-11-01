// bpftime_spin_lock takes the pthread_spinlock_t word inline with glibc's
// protocol, so it excludes against itself and against pthread_spin_lock on
// the same word (map locks are shared with code still calling pthread).
#include <bpf_map/map_common_def.hpp>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <pthread.h>
#include <thread>
#include <vector>

using namespace bpftime;

namespace
{
// True when fn() is still blocked after a while; then the caller unblocks it.
template <class Fn>
bool blocks(Fn fn, std::thread &runner, std::atomic<bool> &done)
{
	runner = std::thread([&] {
		fn();
		done.store(true);
	});
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	return !done.load();
}
} // namespace

TEST_CASE("bpftime spin locks exclude each other and pthread spin locks",
	  "[spin-lock]")
{
	pthread_spinlock_t lock;
	REQUIRE(pthread_spin_init(&lock, PTHREAD_PROCESS_SHARED) == 0);

	SECTION("threads mixing both lock functions lose no increment")
	{
		constexpr int threads = 4, rounds = 200000;
		volatile long counter = 0;
		std::vector<std::thread> workers;
		for (int t = 0; t < threads; t++) {
			workers.emplace_back([&, t] {
				for (int i = 0; i < rounds; i++) {
					const bool ours = (t + i) % 2 == 0;
					if (ours)
						bpftime_spin_lock(&lock);
					else
						pthread_spin_lock(&lock);
					counter = counter + 1;
					if (ours)
						bpftime_spin_unlock(&lock);
					else
						pthread_spin_unlock(&lock);
				}
			});
		}
		for (auto &w : workers)
			w.join();
		REQUIRE(counter == long(threads) * rounds);
	}

	SECTION("pthread_spin_trylock fails while bpftime_spin_lock holds it")
	{
		bpftime_spin_lock(&lock);
		REQUIRE(pthread_spin_trylock(&lock) == EBUSY);
		bpftime_spin_unlock(&lock);
		REQUIRE(pthread_spin_trylock(&lock) == 0);
		pthread_spin_unlock(&lock);
	}

	SECTION("bpftime_spin_lock waits for a pthread_spin_lock holder")
	{
		pthread_spin_lock(&lock);
		std::thread runner;
		std::atomic<bool> done{ false };
		const bool waited = blocks(
			[&] {
				bpftime_spin_lock(&lock);
				bpftime_spin_unlock(&lock);
			},
			runner, done);
		pthread_spin_unlock(&lock);
		runner.join();
		REQUIRE(waited);
		REQUIRE(done.load());
	}

	SECTION("pthread_spin_lock waits for a bpftime_spin_lock holder")
	{
		bpftime_spin_lock(&lock);
		std::thread runner;
		std::atomic<bool> done{ false };
		const bool waited = blocks(
			[&] {
				pthread_spin_lock(&lock);
				pthread_spin_unlock(&lock);
			},
			runner, done);
		bpftime_spin_unlock(&lock);
		runner.join();
		REQUIRE(waited);
		REQUIRE(done.load());
	}
	pthread_spin_destroy(&lock);
}

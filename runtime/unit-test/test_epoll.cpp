/* SPDX-License-Identifier: MIT */
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#if __linux__
#include "bpftime_shm.hpp"
#include "bpftime_shm_internal.hpp"
#include "common_def.hpp"
#include <boost/scope_exit.hpp>
#include <array>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <linux/bpf.h>
#include <pthread.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

TEST_CASE("Epoll returns each ready registration once and restores signals",
	  "[epoll]")
{
	const bool ringbuf = GENERATE(false, true);
	const bool mixed_mask = GENERATE(false, true);
	struct wait_case {
		const char *name;
		int sources;
		int ready;
		int capacity;
		int timeout;
		int expected;
	};
	const auto test = GENERATE(
		wait_case{ "empty instance", 0, 0, 4, 0, 0 },
		wait_case{ "empty source", 1, 0, 4, 0, 0 },
		wait_case{ "ready poll", 1, 1, 4, 0, 1 },
		wait_case{ "ready finite wait", 1, 1, 4, 1000, 1 },
		wait_case{ "two ready polls", 2, 2, 4, 0, 2 },
		wait_case{ "two ready finite waits", 2, 2, 4, 1000, 2 },
		wait_case{ "poll capacity", 2, 2, 1, 0, 1 },
		wait_case{ "finite wait capacity", 2, 2, 1, 1000, 1 },
		wait_case{ "ready indefinite wait", 1, 1, 4, -1, 1 },
		wait_case{ "empty finite wait", 1, 0, 4, 1, 0 });
	CAPTURE(ringbuf, mixed_mask, test.name);

	sigset_t original;
	REQUIRE(pthread_sigmask(SIG_SETMASK, nullptr, &original) == 0);
	sigset_t before = original;
	REQUIRE(sigdelset(&before, SIGINT) == 0);
	REQUIRE(sigdelset(&before, SIGTERM) == 0);
	if (mixed_mask) {
		REQUIRE(sigaddset(&before, SIGTERM) == 0);
		REQUIRE(sigaddset(&before, SIGUSR1) == 0);
	}
	const char *old_name = getenv("BPFTIME_GLOBAL_SHM_NAME");
	const bool had_name = old_name != nullptr;
	const std::string previous_name = old_name ? old_name : "";
	const std::string name = "EpollTestShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(name) };
	std::vector<int> fds;
	bool initialized = false;
	BOOST_SCOPE_EXIT_ALL(&)
	{
		pthread_sigmask(SIG_SETMASK, &original, nullptr);
		if (initialized) {
			for (int fd : fds) {
				if (fd >= 0) {
					bpftime_close(fd);
					close(fd);
				}
			}
			bpftime_destroy_global_shm();
			bpftime_initialize_global_shm(
				bpftime::shm_open_type::SHM_NO_CREATE);
		}
		if (had_name)
			setenv("BPFTIME_GLOBAL_SHM_NAME", previous_name.c_str(),
			       1);
		else
			unsetenv("BPFTIME_GLOBAL_SHM_NAME");
	};
	REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
	bpftime_destroy_global_shm();
	bpftime_initialize_global_shm(
		bpftime::shm_open_type::SHM_REMOVE_AND_CREATE);
	initialized = true;
	const int epoll_fd = bpftime_epoll_create();
	fds.push_back(epoll_fd);
	REQUIRE(epoll_fd >= 0);
	for (int i = 0; i < test.sources; ++i) {
		const int fd =
			ringbuf ? bpftime_maps_create(
					  -1, "epoll_ring",
					  { .type = BPF_MAP_TYPE_RINGBUF,
					    .max_ents = static_cast<uint32_t>(
						    getpagesize()) }) :
				  bpftime_add_software_perf_event(0, 0, 0);
		fds.push_back(fd);
		REQUIRE(fd >= 0);
		const epoll_data_t data{ .u64 = static_cast<uint64_t>(i + 1) };
		if (ringbuf) {
			REQUIRE(bpftime_add_ringbuf_fd_to_epoll(fd, epoll_fd,
								data) == 0);
			if (i < test.ready) {
				auto *payload = bpftime_ringbuf_reserve(
					fd, sizeof(uint64_t));
				REQUIRE(payload != nullptr);
				*static_cast<uint64_t *>(payload) = 42;
				bpftime_ringbuf_submit(fd, payload, 0);
			}
		} else {
			REQUIRE(bpftime_get_software_perf_event_raw_buffer(
					fd, 2 * getpagesize()) != nullptr);
			REQUIRE(bpftime_add_software_perf_event_fd_to_epoll(
					fd, epoll_fd, data) == 0);
			if (i < test.ready) {
				const uint64_t payload = 42;
				REQUIRE(bpftime_perf_event_output(
						fd, &payload,
						sizeof(payload)) == 0);
			}
		}
	}
	// Waiting must not consume data: unread sources remain ready next time.
	for (int repeat = 0; repeat < 2; ++repeat) {
		CAPTURE(repeat);
		std::array<epoll_event, 5> events;
		for (auto &event : events)
			event = { .events = EPOLLOUT, .data = { .u64 = 99 } };
		REQUIRE(pthread_sigmask(SIG_SETMASK, &before, nullptr) == 0);
		const int count = bpftime_epoll_wait(
			epoll_fd, events.data(), test.capacity, test.timeout);
		sigset_t after;
		const int mask_result =
			pthread_sigmask(SIG_SETMASK, nullptr, &after);
		// Restore immediately, including when testing the broken
		// implementation.
		REQUIRE(pthread_sigmask(SIG_SETMASK, &original, nullptr) == 0);
		REQUIRE(mask_result == 0);
		CHECK(count == test.expected);
		for (int signal = 1; signal < NSIG; ++signal) {
			CAPTURE(signal);
			CHECK(sigismember(&after, signal) ==
			      sigismember(&before, signal));
		}
		REQUIRE(count >= 0);
		REQUIRE(count <= test.capacity);
		bool seen[3] = {};
		for (int i = 0; i < count; ++i) {
			CHECK(events[i].events == EPOLLIN);
			const auto id = events[i].data.u64;
			REQUIRE(id >= 1);
			REQUIRE(id <= static_cast<uint64_t>(test.ready));
			CHECK_FALSE(seen[id]);
			seen[id] = true;
		}
		for (size_t i = test.capacity; i < events.size(); ++i) {
			CHECK(events[i].events == EPOLLOUT);
			CHECK(events[i].data.u64 == 99);
		}
	}
}

namespace
{
volatile sig_atomic_t perf_wakeup_sigint_count = 0;

void count_perf_wakeup_sigint(int)
{
	perf_wakeup_sigint_count = perf_wakeup_sigint_count + 1;
}

uint32_t perf_wakeup_waiters()
{
	auto *state = bpftime::shm_holder.global_shared_memory
			      .get_perf_wakeup_state();
	return __atomic_load_n(&state->waiters, __ATOMIC_ACQUIRE);
}

uint32_t perf_wakeup_sequence()
{
	auto *state = bpftime::shm_holder.global_shared_memory
			      .get_perf_wakeup_state();
	return __atomic_load_n(&state->sequence, __ATOMIC_ACQUIRE);
}

bool wait_for_sleeping_consumer()
{
	for (int i = 0; i < 5000; i++) {
		if (perf_wakeup_waiters() > 0)
			return true;
		usleep(1000);
	}
	return false;
}
} // namespace

TEST_CASE("Perf output wakes a sleeping epoll consumer", "[epoll][perf-wakeup]")
{
	const char *old_name = getenv("BPFTIME_GLOBAL_SHM_NAME");
	const bool had_name = old_name != nullptr;
	const std::string previous_name = old_name ? old_name : "";
	const std::string name =
		"EpollPerfWakeupTestShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(name) };
	std::vector<int> fds;
	struct sigaction old_sigint;
	REQUIRE(sigaction(SIGINT, nullptr, &old_sigint) == 0);
	BOOST_SCOPE_EXIT_ALL(&)
	{
		sigaction(SIGINT, &old_sigint, nullptr);
		for (int fd : fds) {
			bpftime_close(fd);
			close(fd);
		}
		bpftime_destroy_global_shm();
		bpftime_initialize_global_shm(
			bpftime::shm_open_type::SHM_NO_CREATE);
		if (had_name)
			setenv("BPFTIME_GLOBAL_SHM_NAME", previous_name.c_str(),
			       1);
		else
			unsetenv("BPFTIME_GLOBAL_SHM_NAME");
	};
	REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
	bpftime_destroy_global_shm();
	bpftime_initialize_global_shm(
		bpftime::shm_open_type::SHM_REMOVE_AND_CREATE);
	REQUIRE(bpftime::shm_holder.global_shared_memory
			.get_perf_wakeup_state() != nullptr);

	const int epoll_fd = bpftime_epoll_create();
	REQUIRE(epoll_fd >= 0);
	fds.push_back(epoll_fd);
	const int perf_fd = bpftime_add_software_perf_event(0, 0, 0);
	REQUIRE(perf_fd >= 0);
	fds.push_back(perf_fd);
	REQUIRE(bpftime_get_software_perf_event_raw_buffer(
			perf_fd, 2 * getpagesize()) != nullptr);
	REQUIRE(bpftime_add_software_perf_event_fd_to_epoll(
			perf_fd, epoll_fd, { .u64 = 7 }) == 0);

	int wait_result = 0;
	int wait_errno = 0;
	epoll_event event{};
	auto start_consumer = [&]() {
		return std::thread([&]() {
			errno = 0;
			wait_result = bpftime_epoll_wait(epoll_fd, &event, 1,
							 10000);
			wait_errno = errno;
		});
	};

	auto *header = (perf_event_mmap_page *)
		bpftime_get_software_perf_event_raw_buffer(perf_fd,
							   2 * getpagesize());
	auto consume_all = [&]() {
		while (bpftime_epoll_wait(epoll_fd, &event, 1, 0) == 1)
			header->data_tail = header->data_head;
	};
	// Returns whether the output went through the futex (the sequence
	// moved) and whether the consumer saw the record.
	auto output_to_sleeping_consumer = [&](const void *data, size_t size) {
		consume_all();
		auto consumer = start_consumer();
		const bool slept = wait_for_sleeping_consumer();
		const uint32_t sequence = perf_wakeup_sequence();
		const int ret = bpftime_perf_event_output(perf_fd, data, size);
		consumer.join();
		REQUIRE(slept);
		REQUIRE(ret == 0);
		CHECK(wait_result == 1);
		CHECK(event.events == EPOLLIN);
		CHECK(event.data.u64 == 7);
		CHECK(perf_wakeup_waiters() == 0);
		return perf_wakeup_sequence() != sequence;
	};

	SECTION("Output at the wakeup watermark wakes the consumer")
	{
		// Nobody is waiting yet: publishing costs no wakeup.
		std::vector<uint8_t> half_ring(getpagesize() / 2);
		const uint32_t idle_sequence = perf_wakeup_sequence();
		REQUIRE(bpftime_perf_event_output(perf_fd, half_ring.data(),
						  half_ring.size()) == 0);
		CHECK(perf_wakeup_sequence() == idle_sequence);

		// The consumer can briefly deregister between 1 ms timeouts;
		// an output landing exactly there is found by the re-check
		// without a wakeup, so allow a few attempts.
		bool woke = false;
		for (int attempt = 0; attempt < 5 && !woke; attempt++)
			woke = output_to_sleeping_consumer(half_ring.data(),
							   half_ring.size());
		CHECK(woke);
	}
	SECTION("Small output is delivered without a wakeup")
	{
		const uint64_t payload = 42;
		CHECK_FALSE(output_to_sleeping_consumer(&payload,
							sizeof(payload)));
	}
	SECTION("Small output wakes a consumer that asks for every sample")
	{
		// libbpf's perf_buffer opens its events with wakeup_events 1.
		const int every_fd = bpftime_add_software_perf_event_with_wakeup(
			0, 0, 0, false, 1);
		REQUIRE(every_fd >= 0);
		fds.push_back(every_fd);
		auto *every_header = (perf_event_mmap_page *)
			bpftime_get_software_perf_event_raw_buffer(
				every_fd, 2 * getpagesize());
		REQUIRE(every_header != nullptr);
		const int every_epoll = bpftime_epoll_create();
		REQUIRE(every_epoll >= 0);
		fds.push_back(every_epoll);
		REQUIRE(bpftime_add_software_perf_event_fd_to_epoll(
				every_fd, every_epoll, { .u64 = 8 }) == 0);
		// As above, a few attempts for an output landing between two
		// timeouts.
		bool woke = false;
		for (int attempt = 0; attempt < 5 && !woke; attempt++) {
			int result = 0;
			epoll_event every_event{};
			std::thread consumer([&]() {
				result = bpftime_epoll_wait(
					every_epoll, &every_event, 1, 10000);
			});
			const bool slept = wait_for_sleeping_consumer();
			const uint32_t sequence = perf_wakeup_sequence();
			const uint64_t payload = 42;
			const int ret = bpftime_perf_event_output(
				every_fd, &payload, sizeof(payload));
			consumer.join();
			REQUIRE(slept);
			REQUIRE(ret == 0);
			CHECK(result == 1);
			CHECK(every_event.data.u64 == 8);
			woke = perf_wakeup_sequence() != sequence;
			every_header->data_tail = every_header->data_head;
		}
		CHECK(woke);
	}
	SECTION("SIGINT interrupts a sleeping consumer")
	{
		struct sigaction act {};
		act.sa_handler = count_perf_wakeup_sigint;
		sigemptyset(&act.sa_mask);
		REQUIRE(sigaction(SIGINT, &act, nullptr) == 0);
		perf_wakeup_sigint_count = 0;
		auto consumer = start_consumer();
		const auto handle = consumer.native_handle();
		const bool slept = wait_for_sleeping_consumer();
		if (slept)
			pthread_kill(handle, SIGINT);
		else
			// Unblock the consumer before failing.
			bpftime_perf_event_output(perf_fd, &slept,
						  sizeof(slept));
		consumer.join();
		REQUIRE(slept);
		CHECK(wait_result == -1);
		CHECK(wait_errno == EINTR);
		CHECK(perf_wakeup_sigint_count == 1);
		CHECK(perf_wakeup_waiters() == 0);
	}
}

// A ring buffer submit used to leave the consumer to its next 1 ms poll: a
// burst larger than the ring buffer within that time lost the rest (opensnoop
// delivered 48% of the opens, uprobe/opensnoop-loss-20261105). bpf's rule: wake
// when the consumer has caught up with the record, unless BPF_RB_NO_WAKEUP;
// always with BPF_RB_FORCE_WAKEUP.
TEST_CASE("Ring buffer records wake a consumer that caught up",
	  "[epoll][ringbuf-wakeup]")
{
	const char *old_name = getenv("BPFTIME_GLOBAL_SHM_NAME");
	const bool had_name = old_name != nullptr;
	const std::string previous_name = old_name ? old_name : "";
	const std::string name =
		"EpollRingbufWakeupTestShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(name) };
	std::vector<int> fds;
	BOOST_SCOPE_EXIT_ALL(&)
	{
		for (int fd : fds) {
			bpftime_close(fd);
			close(fd);
		}
		bpftime_destroy_global_shm();
		bpftime_initialize_global_shm(
			bpftime::shm_open_type::SHM_NO_CREATE);
		if (had_name)
			setenv("BPFTIME_GLOBAL_SHM_NAME", previous_name.c_str(),
			       1);
		else
			unsetenv("BPFTIME_GLOBAL_SHM_NAME");
	};
	REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
	bpftime_destroy_global_shm();
	bpftime_initialize_global_shm(
		bpftime::shm_open_type::SHM_REMOVE_AND_CREATE);
	auto *state = bpftime::shm_holder.global_shared_memory
			      .get_perf_wakeup_state();
	REQUIRE(state != nullptr);

	const int rb_fd = bpftime_maps_create(
		-1, "wakeup_ring",
		{ .type = BPF_MAP_TYPE_RINGBUF, .max_ents = 4096 });
	REQUIRE(rb_fd >= 0);
	fds.push_back(rb_fd);
	auto drain = [&]() {
		return bpftime_poll_from_ringbuf(
			rb_fd, nullptr,
			+[](void *, void *, size_t) -> int { return 0; });
	};
	// Whether submitting one record moved the futex sequence.
	auto submit_one = [&](uint64_t flags, bool discard) {
		void *p = bpftime_ringbuf_reserve(rb_fd, 8);
		REQUIRE(p != nullptr);
		const uint32_t sequence = perf_wakeup_sequence();
		bpftime_ringbuf_submit_flags(rb_fd, p, discard, flags);
		return perf_wakeup_sequence() != sequence;
	};

	SECTION("Submits ask for a wakeup by bpf's rule")
	{
		// Nobody waits: nothing to wake.
		CHECK_FALSE(submit_one(0, false));
		REQUIRE(drain() == 1);
		// A consumer registered as sleeping (as bpftime_epoll_wait
		// does before its futex wait).
		__atomic_add_fetch(&state->waiters, 1, __ATOMIC_SEQ_CST);
		BOOST_SCOPE_EXIT_ALL(&)
		{
			__atomic_sub_fetch(&state->waiters, 1,
					   __ATOMIC_SEQ_CST);
		};
		CHECK(submit_one(0, false)); // the consumer caught up
		CHECK_FALSE(submit_one(0, false)); // it is behind
		CHECK(submit_one(BPF_RB_FORCE_WAKEUP, false));
		REQUIRE(drain() == 3);
		CHECK_FALSE(submit_one(BPF_RB_NO_WAKEUP, false));
		REQUIRE(drain() == 1);
		// The consumer has to pass a discarded record too.
		CHECK(submit_one(0, true));
		CHECK(drain() == 0);
	}
	SECTION("Only ring buffer fds reserve")
	{
		// try_get_ringbuf_map_impl checks the fd and the map type
		// itself (one handler read per call).
		const int array_fd = bpftime_maps_create(
			-1, "not_a_ring",
			{ .type = BPF_MAP_TYPE_ARRAY,
			  .key_size = 4,
			  .value_size = 8,
			  .max_ents = 1 });
		REQUIRE(array_fd >= 0);
		fds.push_back(array_fd);
		CHECK(bpftime_ringbuf_reserve(array_fd, 8) == nullptr);
		CHECK(bpftime_ringbuf_reserve(-1, 8) == nullptr);
		CHECK(bpftime_ringbuf_reserve(1 << 30, 8) == nullptr);
		void *p = bpftime_ringbuf_reserve(rb_fd, 8);
		CHECK(p != nullptr);
		if (p != nullptr)
			bpftime_ringbuf_submit(rb_fd, p, 0);
	}
	SECTION("A record behind the end padding wakes a consumer at the padding")
	{
		// bpftime maps the data area once: a record that does not fit
		// before the end goes to offset 0 behind a padding record
		// filling the end, and a consumer that caught up stands at the
		// padding, not at the record (uprobe/output-dims-20261106,
		// finding 5).
		auto new_ring = [&]() {
			const int fd = bpftime_maps_create(
				-1, "wrap_ring",
				{ .type = BPF_MAP_TYPE_RINGBUF,
				  .max_ents = 4096 });
			REQUIRE(fd >= 0);
			fds.push_back(fd);
			return fd;
		};
		auto fill = [&](int fd, size_t size, int count) {
			for (int i = 0; i < count; i++) {
				void *p = bpftime_ringbuf_reserve(fd, size);
				REQUIRE(p != nullptr);
				bpftime_ringbuf_submit(fd, p, 0);
			}
		};
		auto drain_ring = [&](int fd) {
			return bpftime_poll_from_ringbuf(
				fd, nullptr,
				+[](void *, void *, size_t) -> int { return 0; });
		};
		// Whether submitting one record of this size moved the
		// futex sequence.
		auto submit_sized = [&](int fd, size_t size, uint64_t flags) {
			void *p = bpftime_ringbuf_reserve(fd, size);
			REQUIRE(p != nullptr);
			const uint32_t sequence = perf_wakeup_sequence();
			bpftime_ringbuf_submit_flags(fd, p, false, flags);
			return perf_wakeup_sequence() != sequence;
		};
		__atomic_add_fetch(&state->waiters, 1, __ATOMIC_SEQ_CST);
		BOOST_SCOPE_EXIT_ALL(&)
		{
			__atomic_sub_fetch(&state->waiters, 1,
					   __ATOMIC_SEQ_CST);
		};
		// 1,000-byte records take 1,008 bytes: four end at 4,032, the
		// fifth goes to offset 0 behind 64 bytes of padding.
		int fd = new_ring();
		fill(fd, 1000, 4);
		REQUIRE(drain_ring(fd) == 4);
		CHECK(submit_sized(fd, 1000, 0)); // the consumer is at the padding
		CHECK_FALSE(submit_sized(fd, 1000, 0)); // it is behind
		CHECK(drain_ring(fd) == 2);
		// 48-byte records take 56 bytes: 73 end at 4,088, and the
		// padding of the 74th is a header alone.
		fd = new_ring();
		fill(fd, 48, 73);
		REQUIRE(drain_ring(fd) == 73);
		CHECK(submit_sized(fd, 48, 0));
		CHECK(drain_ring(fd) == 1);
		// BPF_RB_NO_WAKEUP still wakes nobody.
		fd = new_ring();
		fill(fd, 1000, 4);
		REQUIRE(drain_ring(fd) == 4);
		CHECK_FALSE(submit_sized(fd, 1000, BPF_RB_NO_WAKEUP));
		CHECK(drain_ring(fd) == 1);
	}
	SECTION("A record wakes a consumer sleeping in epoll")
	{
		const int epoll_fd = bpftime_epoll_create();
		REQUIRE(epoll_fd >= 0);
		fds.push_back(epoll_fd);
		REQUIRE(bpftime_add_ringbuf_fd_to_epoll(rb_fd, epoll_fd,
							{ .u64 = 9 }) == 0);
		// The consumer can briefly deregister between 1 ms timeouts;
		// a record landing exactly there is found by the re-check
		// without a wakeup, so allow a few attempts.
		bool woke = false;
		for (int attempt = 0; attempt < 5 && !woke; attempt++) {
			drain();
			int wait_result = 0;
			epoll_event event{};
			std::thread consumer([&]() {
				wait_result = bpftime_epoll_wait(epoll_fd, &event,
								 1, 10000);
			});
			const bool slept = wait_for_sleeping_consumer();
			const uint32_t sequence = perf_wakeup_sequence();
			void *p = bpftime_ringbuf_reserve(rb_fd, 8);
			REQUIRE(p != nullptr);
			bpftime_ringbuf_submit(rb_fd, p, 0);
			consumer.join();
			REQUIRE(slept);
			CHECK(wait_result == 1);
			CHECK(event.data.u64 == 9);
			CHECK(perf_wakeup_waiters() == 0);
			woke = perf_wakeup_sequence() != sequence;
		}
		CHECK(woke);
	}
}

// For a while after records the consumer naps registered in `nappers`: perf
// output wakes it only past the watermark or on a drop, not for each sample
// (a FUTEX_WAKE in the probe per sample cost ssl-nginx 10% of its requests
// per second; uprobe/improve-20261106, item 3); ring buffer records by bpf's
// rule as for `waiters`.
TEST_CASE("Napping consumers are woken only when a buffer fills",
	  "[epoll][perf-wakeup][consumer-nap]")
{
	const char *old_name = getenv("BPFTIME_GLOBAL_SHM_NAME");
	const bool had_name = old_name != nullptr;
	const std::string previous_name = old_name ? old_name : "";
	const std::string name =
		"EpollNapTestShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(name) };
	std::vector<int> fds;
	BOOST_SCOPE_EXIT_ALL(&)
	{
		for (int fd : fds) {
			bpftime_close(fd);
			close(fd);
		}
		bpftime_destroy_global_shm();
		bpftime_initialize_global_shm(
			bpftime::shm_open_type::SHM_NO_CREATE);
		if (had_name)
			setenv("BPFTIME_GLOBAL_SHM_NAME", previous_name.c_str(),
			       1);
		else
			unsetenv("BPFTIME_GLOBAL_SHM_NAME");
	};
	REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
	bpftime_destroy_global_shm();
	bpftime_initialize_global_shm(
		bpftime::shm_open_type::SHM_REMOVE_AND_CREATE);
	auto *state = bpftime::shm_holder.global_shared_memory
			      .get_perf_wakeup_state();
	REQUIRE(state != nullptr);
	// libbpf's perf_buffer asks for every sample.
	const int perf_fd =
		bpftime_add_software_perf_event_with_wakeup(0, 0, 0, false, 1);
	REQUIRE(perf_fd >= 0);
	fds.push_back(perf_fd);
	auto *header = (perf_event_mmap_page *)
		bpftime_get_software_perf_event_raw_buffer(perf_fd,
							   2 * getpagesize());
	REQUIRE(header != nullptr);
	auto output_moved_sequence = [&](size_t size) {
		std::vector<uint8_t> payload(size);
		const uint32_t sequence = perf_wakeup_sequence();
		REQUIRE(bpftime_perf_event_output(perf_fd, payload.data(),
						  payload.size()) == 0);
		return perf_wakeup_sequence() != sequence;
	};

	SECTION("Producers' rule for a napping consumer")
	{
		__atomic_add_fetch(&state->nappers, 1, __ATOMIC_SEQ_CST);
		BOOST_SCOPE_EXIT_ALL(&)
		{
			__atomic_sub_fetch(&state->nappers, 1,
					   __ATOMIC_SEQ_CST);
		};
		// A small sample: no wakeup, though the consumer asked for
		// every sample.
		CHECK_FALSE(output_moved_sequence(8));
		// Past half the shard: woken.
		CHECK(output_moved_sequence(getpagesize() / 2));
		// A ring buffer record the consumer has caught up with: woken,
		// as a waiting consumer.
		const int rb_fd = bpftime_maps_create(
			-1, "nap_ring",
			{ .type = BPF_MAP_TYPE_RINGBUF, .max_ents = 4096 });
		REQUIRE(rb_fd >= 0);
		fds.push_back(rb_fd);
		void *p = bpftime_ringbuf_reserve(rb_fd, 8);
		REQUIRE(p != nullptr);
		const uint32_t sequence = perf_wakeup_sequence();
		bpftime_ringbuf_submit(rb_fd, p, 0);
		CHECK(perf_wakeup_sequence() != sequence);
	}
	SECTION("A consumer naps after records")
	{
		const int epoll_fd = bpftime_epoll_create();
		REQUIRE(epoll_fd >= 0);
		fds.push_back(epoll_fd);
		REQUIRE(bpftime_add_software_perf_event_fd_to_epoll(
				perf_fd, epoll_fd, { .u64 = 5 }) == 0);
		std::atomic<int> first_done{ 0 };
		int first = 0, second = 0;
		// One thread: the time of the last records is per thread.
		std::thread consumer([&]() {
			epoll_event event{};
			first = bpftime_epoll_wait(epoll_fd, &event, 1, 10000);
			header->data_tail = header->data_head;
			first_done = 1;
			second = bpftime_epoll_wait(epoll_fd, &event, 1, 10000);
			header->data_tail = header->data_head;
		});
		// Asleep with nothing seen yet: the first sample wakes it.
		REQUIRE(wait_for_sleeping_consumer());
		REQUIRE(bpftime_perf_event_output(perf_fd, &first, 4) == 0);
		bool napping = false;
		for (int i = 0; i < 5000000 && !napping; i++)
			napping = first_done &&
				  __atomic_load_n(&state->nappers,
						  __ATOMIC_ACQUIRE) > 0;
		const uint32_t sequence = perf_wakeup_sequence();
		const int ret = bpftime_perf_event_output(perf_fd, &first, 4);
		consumer.join();
		REQUIRE(napping);
		REQUIRE(ret == 0);
		CHECK(first == 1);
		CHECK(second == 1); // found after the nap
		CHECK(perf_wakeup_sequence() == sequence); // no wakeup
	}
}

#endif

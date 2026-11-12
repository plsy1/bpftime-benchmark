// The host-call watcher's waiting rules (runtime/src/attach/cuda_watcher_policy.hpp),
// without a GPU.
#include "catch2/catch_test_macros.hpp"
#include "attach/cuda_watcher_policy.hpp"
#include <cstdint>

using namespace bpftime::cuda_watcher;

// A window of up to 300 us, doubling while gaps stayed below 300 us, polled
// without pause at request gaps of 55-115 us (a full core); closing it on one
// gap of 50 us or more made 32-call bursts under full load 32-41% dearer per
// call (results gpu/watcher-policy-20261112, v2 and v3).
TEST_CASE("The host-call poll window doubles to 50 us and closes only on two long gaps in a row",
	  "[cuda-watcher-policy]")
{
	poll_window w;
	w.on_request(UINT64_MAX); // the first request
	CHECK(w.window_us() == 0);
	w.on_request(15);
	CHECK(w.window_us() == 10);
	w.on_request(15);
	CHECK(w.window_us() == 20);
	w.on_request(15); // caught by the window
	CHECK(w.window_us() == 20);
	w.on_request(30);
	CHECK(w.window_us() == 40);
	w.on_request(45);
	CHECK(w.window_us() == 50);
	for (int i = 0; i < 100; i++)
		w.on_request(49);
	CHECK(w.window_us() == window_max_us);

	// one long gap keeps the window; a caught gap resets the count
	w.on_request(2500);
	CHECK(w.window_us() == 50);
	w.on_request(12);
	w.on_request(2500);
	CHECK(w.window_us() == 50);
	w.on_request(2500);
	CHECK(w.window_us() == 0);
	w.on_request(30);
	CHECK(w.window_us() == 10);
	// a shorter gap the window did not catch resets the count too
	w.on_request(2500);
	w.on_request(45);
	w.on_request(2500);
	CHECK(w.window_us() == 20);
	w.on_request(2500);
	CHECK(w.window_us() == 0);

	SECTION("gaps of 50 us or more never open it")
	{
		poll_window steady;
		for (uint64_t i = 0; i < 1000; i++) {
			steady.on_request(50 + i % 66);
			REQUIRE(steady.window_us() == 0);
		}
	}
	SECTION("bursts of two calls keep it across the launches between them")
	{
		poll_window two;
		two.on_request(UINT64_MAX);
		for (int launch = 0; launch < 10; launch++) {
			two.on_request(2500); // the launch's first call
			two.on_request(15); // its second
		}
		CHECK(two.window_us() == 20);
	}
}

TEST_CASE("Deferral sleeps follow every request and stop while sleeps return late",
	  "[cuda-watcher-policy]")
{
	deferral d;
	CHECK(!d.take()); // nothing before a request
	d.on_request();
	for (int i = 0; i < deferral_sleeps; i++) {
		REQUIRE(d.take());
		d.on_sleep(deferral_sleep_us, deferral_sleep_us + 4);
	}
	CHECK(!d.take());
	d.on_request();
	CHECK(d.take());

	// sleeps returning 100 us late (a loaded CPU)
	for (int i = 0; i < 20; i++)
		d.on_sleep(deferral_sleep_us, deferral_sleep_us + 104);
	CHECK(d.late_us() > late_guard_us);
	CHECK(!d.take());
	d.on_request();
	CHECK(!d.take());
	// on time again (the slow sleeps measure it): deferral resumes
	for (int i = 0; i < 40; i++)
		d.on_sleep(slow_sleep_us, slow_sleep_us + 4);
	CHECK(d.late_us() < late_guard_us);
	d.on_request();
	CHECK(d.take());
}

TEST_CASE("Two unseen hits within a second latch the watcher and a second without unnoted requests unlatches it",
	  "[cuda-watcher-policy]")
{
	constexpr uint64_t ms = 1000, s = 1'000'000;
	unseen_launch_latch l;
	uint64_t t = 10 * s;
	CHECK(!l.latched(t));
	l.on_hit(t); // a process's first launch: its note comes late
	CHECK(!l.latched(t));
	// hits 2 s apart never latch
	t += 2 * s;
	l.on_hit(t);
	CHECK(!l.latched(t));
	t += 2 * s;
	l.on_hit(t);
	CHECK(!l.latched(t));
	// graph launches every 2.5 ms
	t += 2500;
	l.on_hit(t);
	CHECK(l.latched(t));

	// requests while no noted kernel runs keep it latched
	for (int i = 0; i < 30; i++) {
		t += 100 * ms;
		l.on_request(t, true);
		REQUIRE(l.latched(t));
	}
	// requests during noted launches only: it comes off after 1 s
	const uint64_t last_unnoted = t;
	for (int i = 0; i < 10; i++) {
		t += 100 * ms;
		l.on_request(t, false);
		REQUIRE(l.latched(t));
	}
	t = last_unnoted + unlatch_after_us + 1;
	CHECK(!l.latched(t));
	// unlatched, one more hit alone does not latch it again
	t += 5 * ms;
	l.on_hit(t);
	CHECK(!l.latched(t));
	t += 2500;
	l.on_hit(t);
	CHECK(l.latched(t));

	SECTION("idle while latched")
	{
		CHECK(l.latched(t + unlatch_after_us));
		CHECK(!l.latched(t + unlatch_after_us + 1));
	}
}

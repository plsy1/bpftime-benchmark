#ifndef _BPFTIME_CUDA_WATCHER_POLICY_HPP
#define _BPFTIME_CUDA_WATCHER_POLICY_HPP
// How the agent's host-call watcher waits for GPU requests (results
// gpu/watcher-policy-20261112, set AC3). Pure bookkeeping on microsecond
// timestamps, so the rules can be tested without a GPU; the watcher loop in
// bpf_attach_ctx_cuda.cpp drives them:
//
//   serve a request if one is posted
//   poll while the poll window after the last request is open
//   else take one of the deferral sleeps after a request
//   else park until a launch is noted (1 ms backstop) while no probed kernel
//        may run, or sleep slow_sleep_us while one may
#include <algorithm>
#include <cstdint>

namespace bpftime
{
namespace cuda_watcher
{
constexpr uint64_t window_start_us = 10;
constexpr uint64_t window_max_us = 50;
constexpr int deferral_sleeps = 8;
constexpr uint64_t deferral_sleep_us = 20;
// deferral sleeps returning later than this on average are skipped
constexpr double late_guard_us = 30;
constexpr uint64_t slow_sleep_us = 100;
constexpr uint64_t park_backstop_us = 1000;
constexpr uint64_t event_query_every_us = 1000;
constexpr uint64_t unseen_hit_window_us = 1'000'000;
constexpr uint64_t unlatch_after_us = 1'000'000;

// How long the watcher polls after a request (KVM's halt-poll window). The gap
// is from the end of the previous request's service to the detection of this
// one. A gap the window caught keeps it; a shorter gap than window_max_us
// doubles it (from window_start_us, at most window_max_us); only two gaps of
// window_max_us or more in a row close it. Closing it on one long gap (22.1
// us per call) or halving it (20.7) made 32-call bursts under full CPU load
// dearer than this rule (15.7): the window closed thousands of times per run,
// the late guard skipped the deferral sleeps, and calls waited for
// slow_sleep_us (counted); what makes gaps of 50 us or more inside a burst
// (presumably the watcher's preemption) was not measured. A window of up to
// 300 us polled without pause at request gaps of 55-115 us (a full core).
class poll_window {
    public:
	// gap_us: UINT64_MAX for the first request
	void on_request(uint64_t gap_us) noexcept
	{
		if (gap_us <= window) {
			long_gaps = 0;
		} else if (gap_us < window_max_us) {
			window = std::min(std::max(2 * window, window_start_us),
					  window_max_us);
			long_gaps = 0;
		} else {
			if (long_gaps < 2)
				long_gaps++;
			if (long_gaps >= 2)
				window = 0;
		}
	}
	uint64_t window_us() const noexcept
	{
		return window;
	}

    private:
	uint64_t window = 0;
	int long_gaps = 0;
};

// After a request (and its poll window) the watcher sleeps deferral_sleep_us
// up to deferral_sleeps times before it falls back to slow sleeps or parks
// (NAPI's deferred interrupts). They catch the next call of a burst that the
// window missed. Skipped while sleeps return late (a loaded CPU, where a
// short sleep costs the load a wakeup and still answers late).
class deferral {
    public:
	void on_request() noexcept
	{
		left = deferral_sleeps;
	}
	// whether to take a deferral sleep now
	bool take() noexcept
	{
		if (left <= 0)
			return false;
		if (late_ewma > late_guard_us) {
			left = 0;
			return false;
		}
		left--;
		return true;
	}
	// every sleep of the watcher: asked and slept microseconds
	void on_sleep(uint64_t asked_us, uint64_t slept_us) noexcept
	{
		const double late = (double)slept_us - (double)asked_us - 4.0;
		late_ewma += (late - late_ewma) / 8.0;
	}
	double late_us() const noexcept
	{
		return late_ewma;
	}

    private:
	int left = 0;
	double late_ewma = 0;
};

// Launches the hooks do not note (graph launches, CUDA 13 kernel handles they
// cannot resolve, nvcc 13's default launches) run probed kernels the watcher
// does not know about; it parks, and their calls wait for the 1 ms backstop.
// A hit is a request found waiting when a park ends at its backstop with no
// launch noted. Two hits within unseen_hit_window_us latch the watcher out of
// parking (it sleeps slow_sleep_us between requests as if a kernel ran); the
// latch comes off after unlatch_after_us in which no request was served while
// no noted kernel was in flight. Isolated hits: 29 of 31 in the v3 rounds came
// before the watcher had seen any note (a process's first probed launch, its
// note after the kernel posted; once caught with note() holding its lock); 2
// came under full load 4-5 ms after a note, cause unknown. No run of a
// non-graph application latched.
class unseen_launch_latch {
    public:
	void on_hit(uint64_t now_us) noexcept
	{
		if (!is_latched && last_hit != 0 &&
		    now_us - last_hit <= unseen_hit_window_us) {
			is_latched = true;
			last_evidence = now_us;
		}
		last_hit = now_us;
	}
	// a request served; unnoted: no noted launch was in flight
	void on_request(uint64_t now_us, bool unnoted) noexcept
	{
		if (is_latched && unnoted)
			last_evidence = now_us;
	}
	// whether latched at now_us (comes off here)
	bool latched(uint64_t now_us) noexcept
	{
		if (is_latched && now_us - last_evidence > unlatch_after_us) {
			is_latched = false;
			last_hit = 0;
		}
		return is_latched;
	}

    private:
	bool is_latched = false;
	uint64_t last_hit = 0;
	uint64_t last_evidence = 0;
};
} // namespace cuda_watcher
} // namespace bpftime
#endif

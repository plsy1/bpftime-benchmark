#ifndef _BPFTIME_PROBED_LAUNCH_ACTIVITY_HPP
#define _BPFTIME_PROBED_LAUNCH_ACTIVITY_HPP
// Which probed kernels may be running, for the agent's host-call watcher.
//
// A GPU thread makes host calls only from a probed kernel. The launch hooks
// note every launch of probed code (note()); the watcher sleeps between
// requests while such a launch may still run (with WFE if asked), and parks
// in wait_for_note() otherwise. One object
// per process (launch_activity()), in the attach library: the watcher starts
// before nv_attach_impl is registered.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cuda.h>
#include <mutex>
#include <unordered_map>

namespace bpftime
{
namespace attach
{
class probed_launch_activity {
    public:
	// After a launch of probed code was queued on stream: records an event
	// after it on that stream (one event per stream, recorded again at
	// every launch) and wakes a watcher waiting in wait_for_note(). Nothing
	// for a stream being captured (the launch runs with its graph).
	void note(CUstream stream);
	// Bumped by every note().
	uint64_t generation() const noexcept
	{
		return notes.load(std::memory_order_acquire);
	}
	// Whether a noted launch may still run: an event not yet complete, or
	// the events locked by a note() (never waits for it). Needs a context
	// current (the watcher's primary context). An event that cannot be
	// queried (error, another context) counts as complete and is dropped.
	bool any_running();
	// Waits up to timeout, returning early once generation() differs from
	// seen (a launch was noted meanwhile).
	void wait_for_note(uint64_t seen, std::chrono::microseconds timeout);

    private:
	std::atomic<uint64_t> notes{ 0 };
	std::atomic<int> waiters{ 0 };
	std::mutex wait_mutex;
	std::condition_variable wait_cv;
	std::mutex event_mutex;
	std::unordered_map<CUstream, CUevent> events_by_stream;
};

probed_launch_activity &launch_activity();
} // namespace attach
} // namespace bpftime
#endif

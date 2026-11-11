#include "probed_launch_activity.hpp"
#include <vector>

namespace bpftime
{
namespace attach
{
probed_launch_activity &launch_activity()
{
	// Never destroyed: the watcher and the launch hooks may run during
	// exit.
	static auto *activity = new probed_launch_activity();
	return *activity;
}

void probed_launch_activity::note(CUstream stream)
{
	// A launch into a stream being captured only goes into a graph: it
	// runs at the graph's launch, which the hooks do not see. An event
	// recorded there would be part of the capture, and the watcher querying
	// it would invalidate the application's capture.
	CUstreamCaptureStatus capture = CU_STREAM_CAPTURE_STATUS_NONE;
	if (cuStreamIsCapturing(stream, &capture) != CUDA_SUCCESS ||
	    capture != CU_STREAM_CAPTURE_STATUS_NONE)
		return;
	{
		std::lock_guard<std::mutex> guard(event_mutex);
		auto [it, inserted] = events_by_stream.try_emplace(stream, nullptr);
		if (it->second == nullptr &&
		    cuEventCreate(&it->second, CU_EVENT_DISABLE_TIMING) !=
			    CUDA_SUCCESS) {
			events_by_stream.erase(it);
		} else if (cuEventRecord(it->second, stream) != CUDA_SUCCESS) {
			cuEventDestroy(it->second);
			events_by_stream.erase(it);
		}
	}
	notes.fetch_add(1, std::memory_order_seq_cst);
	// Only a sleeping watcher needs the system call. The waiter registers
	// before checking the generation and the notifier bumps it before
	// reading waiters (both sequentially consistent), so one of them sees
	// the other.
	if (waiters.load(std::memory_order_seq_cst) != 0) {
		std::lock_guard<std::mutex> guard(wait_mutex);
		wait_cv.notify_all();
	}
}

bool probed_launch_activity::any_running()
{
	// note() holds event_mutex across cuEventRecord, which can block while
	// the stream's queue is full, that is until a queued probed kernel ends;
	// that kernel may be waiting for this watcher. Never wait for the lock:
	// busy counts as running.
	std::unique_lock<std::mutex> guard(event_mutex, std::try_to_lock);
	if (!guard.owns_lock())
		return true;
	bool running = false;
	for (auto it = events_by_stream.begin(); it != events_by_stream.end();) {
		CUresult st = cuEventQuery(it->second);
		if (st == CUDA_ERROR_NOT_READY) {
			running = true;
			++it;
		} else if (st == CUDA_SUCCESS) {
			++it; // kept for the stream's next launch
		} else {
			cuEventDestroy(it->second);
			it = events_by_stream.erase(it);
		}
	}
	return running;
}

void probed_launch_activity::wait_for_note(uint64_t seen,
					   std::chrono::microseconds timeout)
{
	std::unique_lock<std::mutex> lock(wait_mutex);
	waiters.fetch_add(1, std::memory_order_seq_cst);
	wait_cv.wait_for(lock, timeout, [&] {
		return notes.load(std::memory_order_seq_cst) != seen;
	});
	waiters.fetch_sub(1, std::memory_order_seq_cst);
}
} // namespace attach
} // namespace bpftime

#include "frida_probe_grace.hpp"
#include <algorithm>
#include <cerrno>
#include <linux/membarrier.h>
#include <mutex>
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>

// The runtime's table also reaches an agent loaded with dlmopen (see
// runtime/src/fork_handlers.hpp); the attach tests link no runtime.
namespace bpftime
{
int register_fork_handlers(void (*prepare)(), void (*parent)(),
			   void (*child)()) __attribute__((weak));
}

using namespace bpftime::attach::probe_grace;

namespace
{
std::mutex registry_mutex;
// Never destroyed: threads may still exit after the static destructors.
std::vector<reader *> &readers()
{
	static auto *list = new std::vector<reader *>();
	return *list;
}
// No destructor: a thread's exit runs other destructors that may call probed
// functions (free) after it would have run. synchronize() deletes the readers
// of exited threads instead.
thread_local reader *self_reader = nullptr;
size_t collect_above = 64;

long membarrier(int cmd)
{
	return syscall(__NR_membarrier, cmd, 0, 0);
}
// Without it a reader has to fence itself after marking its counter.
const bool membarrier_registered =
	membarrier(MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED) == 0;

void lock_for_fork()
{
	registry_mutex.lock();
}
void unlock_after_fork()
{
	registry_mutex.unlock();
}
// Only the forking thread exists in the child: the others' counters would
// never move.
void reset_in_fork_child()
{
	auto &list = readers();
	list.clear();
	if (self_reader != nullptr) {
		self_reader->tid = gettid();
		list.push_back(self_reader);
	}
	registry_mutex.unlock();
}
[[maybe_unused]] const int fork_handlers_status =
	bpftime::register_fork_handlers != nullptr ?
		bpftime::register_fork_handlers(lock_for_fork,
						unlock_after_fork,
						reset_in_fork_child) :
		pthread_atfork(lock_for_fork, unlock_after_fork,
			       reset_in_fork_child);

reader *register_self()
{
	auto *r = new reader;
	r->tid = gettid();
	{
		std::lock_guard guard(registry_mutex);
		readers().push_back(r);
	}
	self_reader = r;
	return r;
}

bool thread_gone(pid_t tid)
{
	return syscall(SYS_tgkill, getpid(), tid, 0) != 0 && errno == ESRCH;
}

// A fork child starts unregistered.
bool barrier_on_other_threads()
{
	if (!membarrier_registered)
		return false;
	if (membarrier(MEMBARRIER_CMD_PRIVATE_EXPEDITED) == 0)
		return true;
	return membarrier(MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED) == 0 &&
	       membarrier(MEMBARRIER_CMD_PRIVATE_EXPEDITED) == 0;
}
} // namespace

// The count's parity, not the depth, decides, and each store may run twice
// with the same value: a signal handler's callback that lands between the
// depth change and the store (in enter or in leave) finds the count odd or
// makes it odd itself, and the interrupted store then writes what is already
// there. Deciding on the depth let such a callback run with an even count,
// outside the grace period (review 2026-11-05; the override handler's
// enter/leave are the exposed ones, Gum guards the listeners).
reader *bpftime::attach::probe_grace::enter()
{
	reader *r = self_reader;
	if (r == nullptr) [[unlikely]]
		r = register_self();
	r->depth++;
	const uint64_t c = r->count.load(std::memory_order_relaxed);
	if ((c & 1) == 0) {
		r->count.store(c + 1, std::memory_order_relaxed);
		// The count before the loads of what this callback runs:
		// synchronize() supplies the barrier.
		if (membarrier_registered)
			std::atomic_signal_fence(std::memory_order_seq_cst);
		else
			std::atomic_thread_fence(std::memory_order_seq_cst);
	}
	return r;
}

void bpftime::attach::probe_grace::leave(reader *self)
{
	if (--self->depth != 0)
		return;
	const uint64_t c = self->count.load(std::memory_order_relaxed);
	if (c & 1)
		self->count.store(c + 1, std::memory_order_release);
}

void bpftime::attach::probe_grace::synchronize()
{
	if (!barrier_on_other_threads())
		std::atomic_thread_fence(std::memory_order_seq_cst);
	std::lock_guard guard(registry_mutex);
	auto &list = readers();
	for (reader *r : list) {
		// A callback that detaches cannot wait for itself.
		if (r == self_reader)
			continue;
		const uint64_t seen = r->count.load(std::memory_order_acquire);
		if ((seen & 1) == 0)
			continue;
		for (unsigned spins = 1;
		     r->count.load(std::memory_order_acquire) == seen;
		     spins++) {
			if (spins % 1024 == 0) {
				if (thread_gone(r->tid))
					break;
				sched_yield();
			}
		}
	}
	// Exited threads run no more callbacks (a reused id keeps its reader).
	if (list.size() > collect_above) {
		auto kept = std::remove_if(list.begin(), list.end(),
					   [](reader *r) {
						   if (r == self_reader ||
						       !thread_gone(r->tid))
							   return false;
						   delete r;
						   return true;
					   });
		list.erase(kept, list.end());
		collect_above = std::max<size_t>(64, list.size() * 2);
	}
}

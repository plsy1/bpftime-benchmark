#ifndef _BPFTIME_PRODUCER_IDENTITY_HPP
#define _BPFTIME_PRODUCER_IDENTITY_HPP
#include <cstdint>

extern "C" uint64_t bpftime_identity_epoch;

namespace bpftime
{
namespace identity_detail
{
// __thread, not thread_local: no TLS wrapper call from other files. One
// object: one TLS address computation in the agent (dlmopen: TLS
// descriptors) for all three fields.
struct identity_cache {
	int pid = -1;
	int64_t tid = -1;
	uint64_t epoch = 0;
};
extern __thread identity_cache cache;
void read_identity(int &pid, int64_t &tid);
} // namespace identity_detail

// The calling thread's (pid, tid): bpf_get_current_pid_tgid, perf output
// shards, ringbuf reservations. getpid() and gettid() are system calls
// (glibc caches neither), so they are kept per thread; the fork child
// handler clears them, and a vfork-like clone (bpftime_identity_epoch, bumped
// by the syscall-trace transformer) makes them checked again. If registering
// the handler failed, they are queried on every call.
inline void current_producer_identity(int &pid, int64_t &tid)
{
	const identity_detail::identity_cache &c = identity_detail::cache;
	if (__builtin_expect(c.pid != -1 &&
				     c.epoch ==
					     __atomic_load_n(&bpftime_identity_epoch,
							     __ATOMIC_RELAXED),
			     1)) {
		pid = c.pid;
		tid = c.tid;
		return;
	}
	identity_detail::read_identity(pid, tid);
}

// The process's pid namespace (inode of /proc/self/ns/pid; 0 if unknown):
// pids mean something only within it. Cached; cleared in fork children (a
// child forked after unshare(CLONE_NEWPID) is in another one).
uint64_t producer_pid_namespace();

// Uncached.
uint64_t current_pid_namespace();
int64_t current_thread_id();

// tgkill(pid, tid, 0): alive unless ESRCH (EPERM: alive). Meaningful only
// for a thread of the caller's pid namespace.
bool is_thread_alive(int pid, int64_t tid);
} // namespace bpftime

#endif

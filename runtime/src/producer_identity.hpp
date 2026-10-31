#ifndef _BPFTIME_PRODUCER_IDENTITY_HPP
#define _BPFTIME_PRODUCER_IDENTITY_HPP
#include <cstdint>

namespace bpftime
{
// The calling thread's (pid, tid): bpf_get_current_pid_tgid, perf output
// shards, ringbuf reservations. getpid() and gettid() are system calls
// (glibc caches neither), so they are kept per thread; the fork child
// handler clears them, and a vfork-like clone (bpftime_identity_epoch, bumped
// by the syscall-trace transformer) makes them checked again. If registering
// the handler failed, they are queried on every call.
void current_producer_identity(int &pid, int64_t &tid);

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

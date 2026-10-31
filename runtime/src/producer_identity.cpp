#include "producer_identity.hpp"
#include "fork_handlers.hpp"
#include <atomic>
#include <cerrno>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

extern "C" {
// Bumped by the syscall-trace transformer before a clone that shares the
// calling thread's memory and TLS (vfork, posix_spawn: CLONE_VM without
// CLONE_SETTLS). Such a child runs on the parent thread's cached identity,
// and no fork handler runs for it; identities cached before the bump are
// checked again. Exported for the transformer (agent.version).
__attribute__((visibility("default"))) uint64_t bpftime_identity_epoch = 0;
}

namespace bpftime
{
namespace
{
thread_local int cached_producer_pid = -1;
thread_local int64_t cached_producer_tid = -1;
thread_local uint64_t cached_producer_epoch = 0;
// This process's pid, refreshed in fork children only: in a vfork child it
// is still the parent's.
int process_pid = getpid();
// UINT64_MAX: not read yet.
std::atomic<uint64_t> cached_producer_namespace{ UINT64_MAX };
const int producer_identity_atfork_status =
	register_fork_handlers(nullptr, nullptr, [] {
		process_pid = getpid();
		cached_producer_pid = -1;
		cached_producer_tid = -1;
		cached_producer_namespace.store(UINT64_MAX,
						std::memory_order_relaxed);
	});
} // namespace

int64_t current_thread_id()
{
#if __linux__
	return (int64_t)syscall(SYS_gettid);
#elif __APPLE__
	uint64_t tid = 0;
	pthread_threadid_np(nullptr, &tid);
	return (int64_t)tid;
#else
	return 0;
#endif
}

uint64_t current_pid_namespace()
{
#if __linux__
	struct stat st;
	if (stat("/proc/self/ns/pid", &st) == 0)
		return (uint64_t)st.st_ino;
#endif
	return 0;
}

bool is_thread_alive(int pid, int64_t tid)
{
#if __linux__
	if (syscall(SYS_tgkill, pid, (pid_t)tid, 0) == 0)
		return true;
	return errno == EPERM;
#else
	(void)pid;
	(void)tid;
	return true;
#endif
}

uint64_t producer_pid_namespace()
{
	uint64_t ns = cached_producer_namespace.load(std::memory_order_relaxed);
	if (ns == UINT64_MAX || producer_identity_atfork_status != 0) {
		ns = current_pid_namespace();
		cached_producer_namespace.store(ns, std::memory_order_relaxed);
	}
	return ns;
}

void current_producer_identity(int &pid, int64_t &tid)
{
	if (producer_identity_atfork_status != 0) {
		pid = getpid();
		tid = current_thread_id();
		return;
	}
	const uint64_t epoch =
		__atomic_load_n(&bpftime_identity_epoch, __ATOMIC_RELAXED);
	if (__builtin_expect(cached_producer_pid != -1 &&
				     cached_producer_epoch == epoch,
			     1)) {
		pid = cached_producer_pid;
		tid = cached_producer_tid;
		return;
	}
	pid = getpid();
	tid = current_thread_id();
	// Another pid than the process's: a vfork child, whose ids are not
	// this thread's. Cache only the thread's own.
	if (pid == process_pid) {
		cached_producer_pid = pid;
		cached_producer_tid = tid;
		cached_producer_epoch = epoch;
	}
}
} // namespace bpftime

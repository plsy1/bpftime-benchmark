#include "producer_identity.hpp"
#include "fork_handlers.hpp"
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
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
namespace identity_detail
{
__thread identity_cache cache;
} // namespace identity_detail
using namespace identity_detail;
namespace
{
// This process's pid, refreshed in fork children only: in a vfork child it
// is still the parent's.
int process_pid = getpid();
// UINT64_MAX: not read yet.
std::atomic<uint64_t> cached_producer_namespace{ UINT64_MAX };
const int producer_identity_atfork_status =
	register_fork_handlers(nullptr, nullptr, [] {
		process_pid = getpid();
		cache.pid = -1;
		cache.tid = -1;
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
	return pid_namespace_id("/proc/self/ns/pid");
}

uint64_t pid_namespace_id(const char *ns_path)
{
#if __linux__
	struct stat st;
	if (stat(ns_path, &st) == 0)
		return (uint64_t)st.st_ino;
#endif
	// Unknown (no /proc): a value no other process has, so that neither
	// shards nor dead-owner recovery take another namespace's pids for
	// this one's (0 for every such process made them equal). Inode
	// numbers do not use the top bit.
	uint64_t id = 0;
#if __linux__
	if (syscall(SYS_getrandom, &id, sizeof(id), 0x1 /* GRND_NONBLOCK */) !=
	    (long)sizeof(id))
#endif
	{
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		id = ((uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec) ^
		     ((uint64_t)getpid() << 40) ^ (uint64_t)(uintptr_t)&ts;
	}
	return id | (1ull << 63);
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

bool is_thread_gone(int pid, int64_t tid)
{
#if __linux__
	if (syscall(SYS_tgkill, pid, (pid_t)tid, 0) != 0)
		return errno == ESRCH;
	// tgkill finds a thread-group leader until its parent reaps it: a
	// main thread that exited is a zombie (Z) or dead (X).
	char path[64];
	snprintf(path, sizeof(path), "/proc/%d/task/%d/stat", pid, (int)tid);
	FILE *f = fopen(path, "re");
	if (f == nullptr)
		return false;
	char buf[512];
	const size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = 0;
	// "pid (comm) S ...": comm may contain spaces and parentheses.
	const char *end = strrchr(buf, ')');
	return end != nullptr && end[1] == ' ' &&
	       (end[2] == 'Z' || end[2] == 'X');
#else
	(void)pid;
	(void)tid;
	return false;
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

// The slow path of current_producer_identity: a cache miss, or registering
// the fork handler failed (then nothing is cached).
void identity_detail::read_identity(int &pid, int64_t &tid)
{
	pid = getpid();
	tid = current_thread_id();
	if (producer_identity_atfork_status != 0)
		return;
	// Another pid than the process's: a vfork child, whose ids are not
	// this thread's. Cache only the thread's own.
	if (pid == process_pid) {
		cache.pid = pid;
		cache.tid = tid;
		cache.epoch =
			__atomic_load_n(&bpftime_identity_epoch, __ATOMIC_RELAXED);
	}
}
} // namespace bpftime

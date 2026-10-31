// Minimal agent for LD_PRELOAD runs: counts hooked calls, bumps nothing, writes the count at exit.
#include <atomic>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cstdint>
using hook_t = int64_t (*)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
extern "C" { uint64_t bpftime_identity_epoch = 0; bool injected_with_frida = false; }
static hook_t orig;
static std::atomic<uint64_t> calls{ 0 };
static int64_t hook(int64_t nr, int64_t a1, int64_t a2, int64_t a3, int64_t a4, int64_t a5, int64_t a6) {
	calls.fetch_add(1, std::memory_order_relaxed);
	return orig(nr, a1, a2, a3, a4, a5, a6);
}
extern "C" void _bpftime__setup_syscall_trace_callback(hook_t *p) { orig = *p; *p = hook; }
extern "C" void bpftime_agent_fork_hook(int) {}
__attribute__((destructor)) static void report() {
	const char *f = getenv("FAKE_COUNT_FILE");
	if (!f) return;
	char buf[128];
	int n = snprintf(buf, sizeof buf, "pid %d calls %lu epoch %lu\n", getpid(), calls.load(), bpftime_identity_epoch);
	int fd = open(f, O_WRONLY | O_APPEND | O_CREAT, 0644);
	if (fd >= 0) { (void)!write(fd, buf, n); close(fd); }
}

// aarch64 system call rewriting: every case forks a child, rewrites the
// child's own code (setup_syscall_tracer is process-wide and irreversible),
// and reports through the exit status. A hook counts the calls and deliberately
// clobbers caller-saved registers, as the agent's C++ dispatch does.
#include "text_segment_transformer.hpp"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cerrno>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <spawn.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern "C" int64_t bpftime_test_svc_with_patterns(int64_t nr, uint64_t *out);
extern "C" void bpftime_test_restorer();
extern char **environ;

namespace
{
std::atomic<uint64_t> calls[512];
std::atomic<int64_t> last_write_fd{ -1 };
std::atomic<uint64_t> last_write_len{ 0 };
syscall_hooker_func_t next_hook;
int64_t fake_getppid = 0;

// Uses SIMD and floating point registers and the condition flags, and makes
// no system call (it runs inside the hook).
__attribute__((noinline)) void clobber_caller_saved()
{
	volatile double acc = 1.0;
	for (int i = 1; i < 8; i++)
		acc = acc * 1.0001 + std::sqrt((double)i);
	char buf[256];
	memset(buf, 0x77, sizeof(buf));
	__asm__ volatile("" : : "r"(buf) : "memory");
	__asm__ volatile("movi v0.16b, #0x33\n\t"
			 "movi v7.16b, #0x33\n\t"
			 "movi v16.16b, #0x33\n\t"
			 "movi v31.16b, #0x33\n\t"
			 "cmp xzr, xzr\n\t"
			 :
			 :
			 : "v0", "v7", "v16", "v31", "cc");
}

int64_t recording_hook(int64_t nr, int64_t a1, int64_t a2, int64_t a3,
		       int64_t a4, int64_t a5, int64_t a6)
{
	if (nr >= 0 && nr < 512)
		calls[nr]++;
	if (nr == SYS_write) {
		last_write_fd = a1;
		last_write_len = (uint64_t)a3;
	}
	clobber_caller_saved();
	if (nr == SYS_getppid && fake_getppid != 0)
		return fake_getppid;
	return next_hook(nr, a1, a2, a3, a4, a5, a6);
}

void install()
{
	bpftime::setup_syscall_tracer();
	next_hook = bpftime::get_call_hook();
	bpftime::set_call_hook(&recording_hook);
}

// Runs `body` in a child process after rewriting it; returns its exit code,
// or 128 + the signal that ended it.
template <typename F> int in_rewritten_child(F body)
{
	fflush(stdout); // the child would print the parent's buffered output
	fflush(stderr);
	pid_t pid = fork();
	if (pid == 0) {
		alarm(30);
		install();
		_exit(body());
	}
	int status = 0;
	waitpid(pid, &status, 0);
	if (WIFSIGNALED(status))
		return 128 + WTERMSIG(status);
	return WEXITSTATUS(status);
}
} // namespace

TEST_CASE("rewritten svc preserves every register but x0",
	  "[syscall-rewrite]")
{
	int code = in_rewritten_child([] {
		uint64_t out[96];
		uint64_t before = calls[SYS_getpid];
		int64_t ret = bpftime_test_svc_with_patterns(SYS_getpid, out);
		if (calls[SYS_getpid] != before + 1)
			return 2; // the svc was not rewritten
		if (ret != getpid())
			return 3;
		for (int n = 1; n <= 30; n++) {
			uint64_t want = n == 8 ? SYS_getpid :
						 (0x5a5aull << 48) | n;
			if (out[n] != want) {
				fprintf(stderr, "x%d = %#llx, want %#llx\n", n,
					(unsigned long long)out[n],
					(unsigned long long)want);
				return 4;
			}
		}
		if ((out[31] >> 28) != 0xa) {
			fprintf(stderr, "nzcv = %#llx\n",
				(unsigned long long)out[31]);
			return 5;
		}
		for (int n = 0; n < 32; n++) {
			if (out[32 + 2 * n] != ((0x1111ull << 48) | n) ||
			    out[33 + 2 * n] != ((0x2222ull << 48) | n)) {
				fprintf(stderr, "q%d = %#llx:%#llx\n", n,
					(unsigned long long)out[33 + 2 * n],
					(unsigned long long)out[32 + 2 * n]);
				return 6;
			}
		}
		return 0;
	});
	REQUIRE(code == 0);
}

TEST_CASE("rewritten libc calls keep arguments, results and errors",
	  "[syscall-rewrite]")
{
	int code = in_rewritten_child([] {
		int fds[2];
		if (pipe(fds) != 0)
			return 2;
		if (write(fds[1], "hello", 5) != 5)
			return 3;
		if (last_write_fd != fds[1] || last_write_len != 5)
			return 4; // the hook did not see write's arguments
		char buf[8] = {};
		if (read(fds[0], buf, sizeof(buf)) != 5 ||
		    memcmp(buf, "hello", 5) != 0)
			return 5;
		errno = 0;
		if (open("/nonexistent/bpftime", O_RDONLY) != -1 ||
		    errno != ENOENT)
			return 6;
		if (calls[SYS_openat] == 0 || calls[SYS_read] == 0)
			return 7;
		// The hook can answer without entering the kernel.
		fake_getppid = 4242;
		if (getppid() != 4242)
			return 8;
		return 0;
	});
	REQUIRE(code == 0);
}

TEST_CASE("threads created after rewriting run and return",
	  "[syscall-rewrite]")
{
	int code = in_rewritten_child([] {
		constexpr int n = 8;
		pthread_t threads[n];
		static std::atomic<int> done{ 0 };
		for (auto &t : threads) {
			if (pthread_create(
				    &t, nullptr,
				    [](void *) -> void * {
					    for (int i = 0; i < 1000; i++)
						    syscall(SYS_getpid);
					    done++;
					    return nullptr;
				    },
				    nullptr) != 0)
				return 2;
		}
		for (auto &t : threads)
			pthread_join(t, nullptr);
		if (done != n)
			return 3;
		if (calls[SYS_getpid] < n * 1000)
			return 4;
		return 0;
	});
	REQUIRE(code == 0);
}

TEST_CASE("fork, clone and posix_spawn children return to the call site",
	  "[syscall-rewrite]")
{
	int code = in_rewritten_child([] {
		pid_t pid = fork();
		if (pid == 0)
			_exit(7);
		int status = 0;
		if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
		    WEXITSTATUS(status) != 7)
			return 2;
		// clone with a child stack (the raw clone system call).
		static char stack[64 * 1024] __attribute__((aligned(16)));
		pid = clone([](void *) -> int { return 9; },
			    stack + sizeof(stack), SIGCHLD, nullptr);
		if (pid < 0 || waitpid(pid, &status, 0) != pid ||
		    !WIFEXITED(status) || WEXITSTATUS(status) != 9)
			return 3;
		// posix_spawn: CLONE_VM | CLONE_VFORK on a separate stack.
		char arg0[] = "true";
		char *argv[] = { arg0, nullptr };
		if (posix_spawn(&pid, "/bin/true", nullptr, nullptr, argv,
				environ) != 0)
			return 4;
		if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) ||
		    WEXITSTATUS(status) != 0)
			return 5;
		return 0;
	});
	REQUIRE(code == 0);
}

TEST_CASE("signal handlers return through rt_sigreturn",
	  "[syscall-rewrite]")
{
	int code = in_rewritten_child([] {
		static volatile sig_atomic_t hits = 0;
		struct sigaction sa = {};
		sa.sa_handler = [](int) {
			hits = hits + 1;
			syscall(SYS_getpid); // a hooked call inside the handler
		};
		sigemptyset(&sa.sa_mask);
		if (sigaction(SIGUSR1, &sa, nullptr) != 0 ||
		    sigaction(SIGALRM, &sa, nullptr) != 0)
			return 2;
		kill(getpid(), SIGUSR1);
		if (hits != 1)
			return 3;
		// A signal interrupting a hooked, blocking call (no SA_RESTART).
		itimerval timer = {};
		timer.it_value.tv_usec = 20000;
		setitimer(ITIMER_REAL, &timer, nullptr);
		timespec ts = { 5, 0 };
		errno = 0;
		if (nanosleep(&ts, nullptr) != -1 || errno != EINTR)
			return 4;
		if (hits != 2)
			return 5;
		return 0;
	});
	REQUIRE(code == 0);
}

TEST_CASE("a program's own signal restorer returns through rt_sigreturn",
	  "[syscall-rewrite]")
{
	int code = in_rewritten_child([] {
		// glibc returns from handlers through the vDSO, which is not
		// rewritten; musl and Go install a restorer in their own code.
		static volatile sig_atomic_t hits = 0;
		struct {
			void (*handler)(int);
			unsigned long flags;
			void (*restorer)();
			uint64_t mask;
		} ksa = {};
		ksa.handler = [](int) { hits = hits + 1; };
		ksa.flags = 0x04000000; // SA_RESTORER
		ksa.restorer = bpftime_test_restorer;
		if (syscall(SYS_rt_sigaction, SIGUSR2, &ksa, nullptr, 8) != 0)
			return 2;
		pid_t self = getpid();
		uint64_t before = calls[SYS_getpid];
		kill(self, SIGUSR2);
		syscall(SYS_getpid); // execution continues after the handler
		if (hits != 1 || calls[SYS_getpid] != before + 1) {
			fprintf(stderr, "hits %d, getpid calls %llu\n", (int)hits,
				(unsigned long long)(calls[SYS_getpid] - before));
			return 3;
		}
		return 0;
	});
	REQUIRE(code == 0);
}

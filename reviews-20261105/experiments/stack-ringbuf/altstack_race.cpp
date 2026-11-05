// A signal handler on an alternate stack that makes a hooked system call,
// arriving in the hook's epilogue after `busy` is cleared and before the
// frame's registers are read back: its hook switches to the top of the hook
// stack and builds its frame over the live one.
//
// Modes: 3 = altstack handler making a hooked call (expected to fail),
//        2 = handler without altstack making a hooked call (control),
//        1 = altstack handler making no call (control).
#include "harness.hpp"
#include <atomic>
#include <ucontext.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>

struct shared_state {
	std::atomic<uint64_t> victim_iters, handler_runs, signals_sent, bad;
	std::atomic<uint64_t> window_hits, last_pc_off, in_hook_hits;
	std::atomic<uint64_t> pair[4]; // (sp in range) * 2 + (busy != 0)
	std::atomic<uint64_t> hit_pc[16];
	std::atomic<uint64_t> first_hit_iter, first_hit_runs, first_hit_signals;
	std::atomic<int> done;
};
static shared_state *S;
static int mode;
static int spin = 20000;

// The transformer's per-thread hook stack state (hidden TLS, reachable here
// because the core library is linked statically) and the asm hook.
struct hook_stack_state {
	uint64_t lo, hi, busy;
};
extern "C" __thread hook_stack_state bpftime_hook_stack;
extern "C" void syscall_hooker_asm();

static void handler(int, siginfo_t *, void *ctx)
{
	S->handler_runs.fetch_add(1, std::memory_order_relaxed);
	auto *uc = (ucontext_t *)ctx;
	const uint64_t sp = uc->uc_mcontext.sp, pc = uc->uc_mcontext.pc;
	const uint64_t off = pc - (uint64_t)&syscall_hooker_asm;
	if (off < 1024)
		S->in_hook_hits.fetch_add(1, std::memory_order_relaxed);
	// Interrupted with sp on the hook stack (a frame live there) while
	// busy is already 0: our hooked call below will switch to the top.
	const bool in_range = sp >= bpftime_hook_stack.lo &&
			      sp < bpftime_hook_stack.hi;
	S->pair[in_range * 2 + (bpftime_hook_stack.busy != 0)].fetch_add(1);
	if (in_range && bpftime_hook_stack.busy == 0) {
		uint64_t n = S->window_hits.fetch_add(1, std::memory_order_relaxed);
		if (n < 16)
			S->hit_pc[n].store(off);
		if (n == 0) {
			S->first_hit_iter.store(S->victim_iters.load());
			S->first_hit_runs.store(S->handler_runs.load());
			S->first_hit_signals.store(S->signals_sent.load());
		}
		S->last_pc_off.store(off, std::memory_order_relaxed);
	}
	if (mode & 2)
		syscall(SYS_getppid);
}

static void *signaller(void *arg)
{
	pthread_t victim = *(pthread_t *)arg;
	while (!S->done.load()) {
		pthread_kill(victim, SIGUSR1);
		S->signals_sent.fetch_add(1, std::memory_order_relaxed);
		for (volatile int i = 0; i < spin; i++)
			;
	}
	return nullptr;
}

static int run_child(uint64_t iterations)
{
	install_hook();
	getppid(); // the first traced call maps the hook stack
	if (mode & 1) {
		static uint8_t alt[64 << 10];
		stack_t ss = {};
		ss.ss_sp = alt;
		ss.ss_size = sizeof(alt);
		if (sigaltstack(&ss, nullptr) != 0)
			return 10;
	}
	struct sigaction sa = {};
	sa.sa_sigaction = handler;
	sa.sa_flags = SA_RESTART | SA_SIGINFO | ((mode & 1) ? SA_ONSTACK : 0);
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGUSR1, &sa, nullptr) != 0)
		return 11;
	pthread_t self = pthread_self(), t;
	if (pthread_create(&t, nullptr, signaller, &self) != 0)
		return 12;
	const pid_t ppid = getppid();
	for (uint64_t i = 0; i < iterations; i++) {
		if (getppid() != ppid)
			S->bad.fetch_add(1);
		S->victim_iters.store(i + 1, std::memory_order_relaxed);
	}
	S->done.store(1);
	pthread_join(t, nullptr);
	return S->bad.load() ? 13 : 0;
}

int main(int argc, char **argv)
{
	mode = argc > 1 ? atoi(argv[1]) : 3;
	spin = argc > 3 ? atoi(argv[3]) : 20000;
	const uint64_t iterations = argc > 2 ? strtoull(argv[2], nullptr, 10) :
					       2000000;
	S = (shared_state *)mmap(nullptr, sizeof(*S), PROT_READ | PROT_WRITE,
				 MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	new (S) shared_state{};
	fflush(stdout);
	pid_t pid = fork();
	if (pid == 0)
		_exit(run_child(iterations));
	uint64_t last = 0;
	int stalled = 0;
	for (int tick = 0; tick < 600; tick++) {
		int status;
		pid_t r = waitpid(pid, &status, WNOHANG);
		if (r == pid) {
			printf("mode %d: child exited: %s %d; victim iters %lu, handler runs %lu, signals %lu, bad %lu, in-hook landings %lu, window hits %lu\n",
			       mode,
			       WIFSIGNALED(status) ? "signal" : "code",
			       WIFSIGNALED(status) ? WTERMSIG(status) :
						     WEXITSTATUS(status),
			       (unsigned long)S->victim_iters.load(),
			       (unsigned long)S->handler_runs.load(),
			       (unsigned long)S->signals_sent.load(),
			       (unsigned long)S->bad.load(),
			       (unsigned long)S->in_hook_hits.load(),
			       (unsigned long)S->window_hits.load());
			return WIFSIGNALED(status) ? 2 : WEXITSTATUS(status);
		}
		struct timespec ts = { 0, 100000000 };
		nanosleep(&ts, nullptr);
		uint64_t now = S->victim_iters.load();
		if (now == last)
			stalled++;
		else
			stalled = 0;
		last = now;
		if (stalled >= 30) {
			printf("mode %d: victim STALLED for 3 s at iteration %lu: handler runs %lu, signals sent %lu, bad %lu; signals landing inside syscall_hooker_asm %lu, of them with sp on the hook stack and busy == 0: %lu (last at syscall_hooker_asm+%lu)\n",
			       mode, (unsigned long)now,
			       (unsigned long)S->handler_runs.load(),
			       (unsigned long)S->signals_sent.load(),
			       (unsigned long)S->bad.load(),
			       (unsigned long)S->in_hook_hits.load(),
			       (unsigned long)S->window_hits.load(),
			       (unsigned long)S->last_pc_off.load());
			printf("  (sp off hook stack, busy 0) %lu, (off, busy 1) %lu, (on, busy 0) %lu, (on, busy 1) %lu; first hit pcs:",
			       (unsigned long)S->pair[0].load(), (unsigned long)S->pair[1].load(),
			       (unsigned long)S->pair[2].load(), (unsigned long)S->pair[3].load());
			for (int k = 0; k < 16; k++)
				printf(" +%lu", (unsigned long)S->hit_pc[k].load());
			printf("\n  first hit: at victim iteration %lu, handler run %lu, signals sent %lu\n",
			       (unsigned long)S->first_hit_iter.load(),
			       (unsigned long)S->first_hit_runs.load(),
			       (unsigned long)S->first_hit_signals.load());
			char cmd[256];
			snprintf(cmd, sizeof(cmd), "gdb -p %d -batch -ex 'info registers pc sp x16 x17' -ex 'x/3i $pc' -ex 'bt 6' 2>&1 | grep -v '^\\[' | head -30", (int)pid);
			fflush(stdout);
			if (system(cmd) == -1) {}
			kill(pid, SIGKILL);
			waitpid(pid, nullptr, 0);
			return 3;
		}
	}
	printf("mode %d: timeout\n", mode);
	kill(pid, SIGKILL);
	waitpid(pid, nullptr, 0);
	return 4;
}

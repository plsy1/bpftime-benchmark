// Plain program (no bpftime code linked): run under
//   LD_PRELOAD=<build>/libbpftime-agent-transformer.so AGENT_SO=/nonexistent
// so the shipped transformer rewrites it and dispatches through its default
// pass-through hook.
//   race [spin]: an SA_ONSTACK handler making a system call while the main
//                thread loops on getppid (expected: the main thread stalls)
//   spawn:       bytes a hooked getpid writes below sp before and after a
//                posix_spawn (expected: 32 before, ~800+ after: busy stuck)
#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
extern char **environ;

struct shared_state {
	std::atomic<uint64_t> victim_iters, handler_runs, signals_sent, bad;
	std::atomic<int> done;
};
static shared_state *S;
static int spin = 2000;

static void handler(int)
{
	S->handler_runs.fetch_add(1, std::memory_order_relaxed);
	syscall(SYS_getppid);
}

static void *signaller(void *arg)
{
	pthread_t victim = *(pthread_t *)arg;
	while (!S->done.load()) {
		pthread_kill(victim, SIGUSR1);
		S->signals_sent.fetch_add(1, std::memory_order_relaxed);
		for (volatile int i = 0; i < spin; i = i + 1)
			;
	}
	return nullptr;
}

static int race_child()
{
	static uint8_t alt[64 << 10];
	stack_t ss = {};
	ss.ss_sp = alt;
	ss.ss_size = sizeof(alt);
	if (sigaltstack(&ss, nullptr) != 0)
		return 10;
	struct sigaction sa = {};
	sa.sa_handler = handler;
	sa.sa_flags = SA_RESTART | SA_ONSTACK;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGUSR1, &sa, nullptr) != 0)
		return 11;
	pthread_t self = pthread_self(), t;
	if (pthread_create(&t, nullptr, signaller, &self) != 0)
		return 12;
	const pid_t ppid = getppid();
	for (uint64_t i = 0; i < 50000000; i++) {
		if (getppid() != ppid)
			S->bad.fetch_add(1);
		S->victim_iters.store(i + 1, std::memory_order_relaxed);
	}
	S->done.store(1);
	pthread_join(t, nullptr);
	return S->bad.load() ? 13 : 0;
}

static int race()
{
	S = (shared_state *)mmap(nullptr, sizeof(*S), PROT_READ | PROT_WRITE,
				 MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	new (S) shared_state{};
	fflush(stdout);
	pid_t pid = fork();
	if (pid == 0)
		_exit(race_child());
	uint64_t last = 0;
	int stalled = 0;
	for (int tick = 0; tick < 600; tick++) {
		int status;
		if (waitpid(pid, &status, WNOHANG) == pid) {
			printf("race: child exited: %s %d; victim iters %lu, handler runs %lu, signals %lu, bad %lu\n",
			       WIFSIGNALED(status) ? "signal" : "code",
			       WIFSIGNALED(status) ? WTERMSIG(status) :
						     WEXITSTATUS(status),
			       (unsigned long)S->victim_iters.load(),
			       (unsigned long)S->handler_runs.load(),
			       (unsigned long)S->signals_sent.load(),
			       (unsigned long)S->bad.load());
			return WIFSIGNALED(status) ? 2 : WEXITSTATUS(status);
		}
		struct timespec ts = { 0, 100000000 };
		nanosleep(&ts, nullptr);
		uint64_t now = S->victim_iters.load();
		stalled = now == last ? stalled + 1 : 0;
		last = now;
		if (stalled >= 30) {
			printf("race: victim STALLED for 3 s at iteration %lu: handler runs %lu, signals sent %lu, bad %lu\n",
			       (unsigned long)now,
			       (unsigned long)S->handler_runs.load(),
			       (unsigned long)S->signals_sent.load(),
			       (unsigned long)S->bad.load());
			kill(pid, SIGKILL);
			waitpid(pid, nullptr, 0);
			return 3;
		}
	}
	printf("race: timeout\n");
	kill(pid, SIGKILL);
	waitpid(pid, nullptr, 0);
	return 4;
}

__attribute__((noinline)) static long getpid_with_sp(uint8_t *sp)
{
	register long x8 __asm__("x8") = SYS_getpid;
	register long x0 __asm__("x0");
	register uint8_t *x9 __asm__("x9") = sp;
	__asm__ volatile("mov x10, sp\n\t"
			 "mov sp, x9\n\t"
			 "svc #0\n\t"
			 "mov sp, x10\n\t"
			 : "=r"(x0)
			 : "r"(x8), "r"(x9)
			 : "x10", "memory");
	return x0;
}
static uint8_t probe_stack[64 << 10] __attribute__((aligned(16)));
static size_t bytes_used_below_sp()
{
	memset(probe_stack, 0xa5, sizeof(probe_stack));
	getpid_with_sp(probe_stack + sizeof(probe_stack));
	for (size_t i = 0; i < sizeof(probe_stack); i++)
		if (probe_stack[i] != 0xa5)
			return sizeof(probe_stack) - i;
	return 0;
}

static int spawn()
{
	getppid();
	size_t before = bytes_used_below_sp();
	pid_t pid;
	char *args[] = { (char *)"/bin/true", nullptr };
	if (posix_spawn(&pid, "/bin/true", nullptr, nullptr, args, environ) !=
	    0)
		return 10;
	waitpid(pid, nullptr, 0);
	size_t after = bytes_used_below_sp();
	printf("spawn: bytes below sp before %zu, after posix_spawn %zu\n",
	       before, after);
	return 0;
}

int main(int argc, char **argv)
{
	const char *mode = argc > 1 ? argv[1] : "race";
	if (argc > 2)
		spin = atoi(argv[2]);
	if (!strcmp(mode, "race"))
		return race();
	return spawn();
}

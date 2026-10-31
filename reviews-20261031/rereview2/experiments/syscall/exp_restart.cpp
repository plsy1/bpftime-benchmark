#include "common.hpp"
#include <pthread.h>
#include <atomic>
static std::atomic<int> handled{ 0 };
static void h(int, siginfo_t *, void *) { handled++; }
static int fds[2];
static pid_t main_tid;
static void *sender(void *)
{
	usleep(200000);
	syscall(SYS_tgkill, getpid(), main_tid, SIGSYS);
	usleep(200000);
	char c = 'x';
	write(fds[1], &c, 1);
	return nullptr;
}
int main(int argc, char **argv)
{
	// argv[1]: 'l' lazy mode, 'r' rewritten only, 'n' native; argv[2]: 'h' handler SA_RESTART, 'i' SIG_IGN
	char mode = argv[1][0], act = argv[2][0];
	if (mode == 'l') setenv("BPFTIME_SYSCALL_LAZY", "1", 1);
	if (mode != 'n') bpftime::setup_syscall_tracer();
	if (mode == 'l' && !bpftime::enable_lazy_rewriting({})) return 10;
	struct sigaction sa = {};
	if (act == 'h') { sa.sa_sigaction = h; sa.sa_flags = SA_SIGINFO | SA_RESTART; }
	else sa.sa_handler = SIG_IGN;
	if (sigaction(SIGSYS, &sa, nullptr) != 0) return 11;
	pipe(fds);
	main_tid = gettid();
	pthread_t t; pthread_create(&t, nullptr, sender, nullptr);
	char c; errno = 0;
	ssize_t n = read(fds[0], &c, 1);
	int e = errno;
	pthread_join(t, nullptr);
	printf("mode %c action %c: read -> %zd errno=%d (%s) handler runs=%d\n", mode, act, n, e, n < 0 ? strerror(e) : "ok", handled.load());
	return 0;
}

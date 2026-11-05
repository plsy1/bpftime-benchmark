// Does the thread keep its hook stack after events whose hooked call never
// returns through the hook's epilogue (which is the only place `busy` is
// cleared)? Measures bytes written below sp by a hooked call before and after:
// 32 means the hook switched to its own stack, ~880 means it stayed on the
// caller's (busy stuck).
//
// Modes: spawn (posix_spawn: vfork child execs through the shared hook
// stack), system, vfork (child _exits), longjmp (handler siglongjmps out of a
// hooked blocking read), none (control).
#include "harness.hpp"
#include <csetjmp>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <spawn.h>
#include <sys/wait.h>
#include <thread>
extern char **environ;

static sigjmp_buf jb;
static void jump_handler(int)
{
	siglongjmp(jb, 1);
}

int main(int argc, char **argv)
{
	const char *mode = argc > 1 ? argv[1] : "spawn";
	install_hook();
	getppid(); // first traced call: maps the hook stack
	size_t before = bytes_used_below_sp();
	if (!strcmp(mode, "spawn")) {
		pid_t pid;
		char *args[] = { (char *)"/bin/true", nullptr };
		int rc = posix_spawn(&pid, "/bin/true", nullptr, nullptr, args,
				     environ);
		if (rc != 0)
			return 10;
		waitpid(pid, nullptr, 0);
	} else if (!strcmp(mode, "system")) {
		if (system("true") != 0)
			return 11;
	} else if (!strcmp(mode, "vfork")) {
		pid_t pid = vfork();
		if (pid == 0)
			_exit(0);
		waitpid(pid, nullptr, 0);
	} else if (!strcmp(mode, "longjmp")) {
		int fds[2];
		if (pipe(fds) != 0)
			return 12;
		struct sigaction sa = {};
		sa.sa_handler = jump_handler;
		sigemptyset(&sa.sa_mask);
		sigaction(SIGUSR1, &sa, nullptr);
		pthread_t self = pthread_self();
		std::thread killer([self] {
			usleep(100000);
			pthread_kill(self, SIGUSR1);
		});
		if (sigsetjmp(jb, 1) == 0) {
			char c;
			read(fds[0], &c, 1); // blocks in a hooked call
			return 13;
		}
		killer.join();
	}
	size_t after = bytes_used_below_sp();
	size_t after2 = bytes_used_below_sp();
	printf("%s: bytes below sp before %zu, after %zu (next call %zu)\n",
	       mode, before, after, after2);
	return after > 64 ? 1 : 0;
}

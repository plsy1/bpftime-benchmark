#include "common.hpp"
#include <spawn.h>
extern char **environ;
int main(int argc, char **argv)
{
	char mode = argc > 1 ? argv[1][0] : 'n';
	static uint64_t epoch = 0;
	bpftime::setup_syscall_tracer();
	bpftime::set_identity_epoch(&epoch);
	if (mode == 'e' && !install_filter(SYS_process_vm_readv, SECCOMP_RET_ERRNO | EPERM))
		return 10;
	if (mode == 'k' && !install_filter(SYS_process_vm_readv, SECCOMP_RET_KILL_PROCESS))
		return 10;
	char arg0[] = "true";
	char *args[] = { arg0, nullptr };
	pid_t pid; int status = 0;
	if (posix_spawn(&pid, "/bin/true", nullptr, nullptr, args, environ) != 0 || waitpid(pid, &status, 0) != pid)
		return 11;
	printf("mode %c: epoch after posix_spawn (clone3) = %lu (expected 1)\n", mode, (unsigned long)epoch);
	pid = vfork();
	if (pid == 0) _exit(0);
	waitpid(pid, &status, 0);
	printf("mode %c: epoch after vfork (clone) = %lu (expected 2)\n", mode, (unsigned long)epoch);
	return 0;
}

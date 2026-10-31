// The parent, vfork children and raw clone(SIGCHLD) children each stat a
// path naming their own pid: a tool reporting the caller's pid (statsnoop)
// must report the pid in the path. Neither kind of child runs a fork
// handler; a vfork child also shares the parent thread's TLS, where the
// agent caches the identity, and a raw clone child has a copy of it.
#define _GNU_SOURCE
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
static void work(int n)
{
	char path[64];
	snprintf(path, sizeof(path), "/tmp/fork-e2e-%d", getpid());
	struct statx sx;
	for (int i = 0; i < n; i++) {
		statx(AT_FDCWD, path, 0, STATX_BASIC_STATS, &sx);
		if (i % 100 == 0)
			usleep(1000); // let the consumer keep up
	}
}
int main()
{
	work(1); // the parent's identity and shard are cached first
	for (int c = 0; c < 4; c++) {
		pid_t pid = c % 2 == 0 ? vfork() :
					 (pid_t)syscall(SYS_clone, SIGCHLD, 0, 0,
							0, 0);
		if (pid == 0) {
			work(200);
			_exit(0);
		}
		waitpid(pid, NULL, 0);
		work(200);
	}
	return 0;
}

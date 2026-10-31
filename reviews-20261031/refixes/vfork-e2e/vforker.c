// Parent and vfork children each stat a path naming their own pid: a tool
// reporting the caller's pid (statsnoop) must report the pid in the path. A
// vfork child shares the parent thread's TLS, where the agent caches the
// identity, and no fork handler runs for it.
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
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
	work(1); // the parent's identity and shard are cached before the vfork
	for (int c = 0; c < 4; c++) {
		pid_t pid = vfork();
		if (pid == 0) {
			work(200);
			_exit(0);
		}
		waitpid(pid, NULL, 0);
		work(200);
	}
	return 0;
}

// Parent and fork children each stat a path naming their own pid: a tool
// reporting the caller's pid (statsnoop) must report the pid in the path.
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/wait.h>
#include <unistd.h>
static void work(int n)
{
	char path[64];
	snprintf(path, sizeof(path), "/tmp/fork-e2e-%d", getpid());
	struct statx sx;
	struct statfs sf;
	for (int i = 0; i < n; i++) {
		statx(AT_FDCWD, path, 0, STATX_BASIC_STATS, &sx);
		statfs(path, &sf);
		if (i % 100 == 0)
			usleep(1000); // let the consumer keep up
	}
}
int main()
{
	work(1); // the parent's identity and shard are cached before the fork
	for (int c = 0; c < 4; c++)
		if (fork() == 0) {
			work(2000);
			_exit(0);
		}
	work(2000);
	while (wait(NULL) > 0)
		;
	return 0;
}

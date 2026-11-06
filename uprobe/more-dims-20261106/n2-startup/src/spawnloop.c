// Starts N children one after another (posix_spawn, then waitpid) and prints the wall time of
// each from just before the spawn to the reaped exit: p50/p90/p99/mean/max in us, and how many
// failed (non-zero exit or signal). The children get CHILD_PRELOAD (if set) as LD_PRELOAD and
// BPFTIME_LOG_OUTPUT=console; spawnloop itself runs without them (it is not traced).
// Usage: spawnloop N CMD [ARGS...]  (more-dims-20261106/n2-startup)
#define _GNU_SOURCE
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

extern char **environ;

static unsigned long mono_ns(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000UL + t.tv_nsec;
}
static int cmp(const void *a, const void *b)
{
	unsigned long x = *(const unsigned long *)a, y = *(const unsigned long *)b;
	return (x > y) - (x < y);
}

int main(int argc, char **argv)
{
	if (argc < 3)
		return 1;
	int n = atoi(argv[1]);
	const char *pre = getenv("CHILD_PRELOAD");
	char *envp[8];
	int k = 0;
	char a[4096];
	if (pre && *pre) {
		snprintf(a, sizeof(a), "LD_PRELOAD=%s", pre);
		envp[k++] = a;
		envp[k++] = "BPFTIME_LOG_OUTPUT=console";
	}
	envp[k++] = "PATH=/usr/bin:/bin";
	envp[k] = NULL;
	unsigned long *t = calloc(n, sizeof(*t)), sum = 0;
	int failed = 0;
	posix_spawn_file_actions_t fa;
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_addopen(&fa, 1, "/dev/null", 1, 0);
	posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", 1, 0);
	for (int i = 0; i < n; i++) {
		pid_t pid;
		int st;
		unsigned long t0 = mono_ns();
		if (posix_spawn(&pid, argv[2], &fa, NULL, argv + 2, envp)) {
			failed++;
			continue;
		}
		waitpid(pid, &st, 0);
		t[i] = mono_ns() - t0;
		sum += t[i];
		if (!WIFEXITED(st) || WEXITSTATUS(st))
			failed++;
	}
	qsort(t, n, sizeof(*t), cmp);
	printf("spawn_n %d\nspawn_failed %d\nspawn_p50_us %.1f\nspawn_p90_us %.1f\n"
	       "spawn_p99_us %.1f\nspawn_mean_us %.1f\nspawn_max_us %.1f\n",
	       n, failed, t[(n - 1) / 2] / 1e3, t[(n - 1) * 9 / 10] / 1e3, t[(n - 1) * 99 / 100] / 1e3,
	       sum / 1e3 / n, t[n - 1] / 1e3);
	return 0;
}

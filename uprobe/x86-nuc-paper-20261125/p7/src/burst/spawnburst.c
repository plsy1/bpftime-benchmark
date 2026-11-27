// Starts K children at once (K posix_spawn back to back), waits for all of them, R rounds.
// Prints the wall time of each child from just before its spawn to its reaped exit and its CPU
// time (wait4 rusage, user + system): p50/p90/max over all children, and the round's wall time
// (first spawn to last exit) p50/max. The children get CHILD_PRELOAD (if set) as LD_PRELOAD and
// BPFTIME_LOG_OUTPUT=console; spawnburst itself is never traced.
// Usage: spawnburst K R CMD [ARGS...]  (remeasure-20261108/concurrent-start)
#define _GNU_SOURCE
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
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
static void stats(const char *name, unsigned long *v, int n)
{
	qsort(v, n, sizeof(*v), cmp);
	printf("%s_p50_ms %.2f\n%s_p90_ms %.2f\n%s_max_ms %.2f\n", name, v[(n - 1) / 2] / 1e6,
	       name, v[(n - 1) * 9 / 10] / 1e6, name, v[n - 1] / 1e6);
}

int main(int argc, char **argv)
{
	if (argc < 4)
		return 1;
	int k = atoi(argv[1]), r = atoi(argv[2]);
	const char *pre = getenv("CHILD_PRELOAD");
	char *envp[8];
	int e = 0;
	char a[4096];
	if (pre && *pre) {
		snprintf(a, sizeof(a), "LD_PRELOAD=%s", pre);
		envp[e++] = a;
		envp[e++] = "BPFTIME_LOG_OUTPUT=console";
	}
	envp[e++] = "PATH=/usr/bin:/bin";
	envp[e] = NULL;
	posix_spawn_file_actions_t fa;
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_addopen(&fa, 1, "/dev/null", 1, 0);
	posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", 1, 0);
	int n = k * r, failed = 0, c = 0;
	unsigned long *wall = calloc(n, sizeof(long)), *cpu = calloc(n, sizeof(long)),
		      *round = calloc(r, sizeof(long)), *t0 = calloc(k, sizeof(long));
	pid_t *pid = calloc(k, sizeof(pid_t));
	for (int j = 0; j < r; j++) {
		unsigned long start = mono_ns(), last = start;
		for (int i = 0; i < k; i++) {
			t0[i] = mono_ns();
			if (posix_spawn(&pid[i], argv[3], &fa, NULL, argv + 3, envp)) {
				pid[i] = -1;
				failed++;
			}
		}
		for (int left = k; left > 0; left--) {
			int st;
			struct rusage ru;
			pid_t p = wait4(-1, &st, 0, &ru);
			unsigned long now = mono_ns();
			if (p < 0)
				break;
			for (int i = 0; i < k; i++)
				if (pid[i] == p) {
					wall[c] = now - t0[i];
					cpu[c] = (ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) * 1000000000UL +
						 (ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) * 1000UL;
					c++;
				}
			last = now;
			if (!WIFEXITED(st) || WEXITSTATUS(st))
				failed++;
		}
		round[j] = last - start;
	}
	printf("burst_k %d\nburst_rounds %d\nburst_children %d\nburst_failed %d\n", k, r, c, failed);
	stats("child_wall", wall, c);
	stats("child_cpu", cpu, c);
	stats("round_wall", round, r);
	return 0;
}

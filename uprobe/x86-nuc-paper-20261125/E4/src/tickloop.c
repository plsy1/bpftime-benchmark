// E4 target: T threads each call the noinline tick() N times. The whole process is bound to a
// CPU set (default 2-7) with sched_setaffinity before the threads start; threads are not pinned
// individually, so T > 6 oversubscribes and the scheduler may preempt and migrate them.
// Threads start together after a barrier. Prints one JSON line:
//   {"T":..,"N":..,"cpus":"2-7","wall_s":..,"expected":T*N}
// expected = T*N is the number of tick() calls by construction (each thread runs the full loop);
// whether every call reached the BPF program is checked by the program's own run counter.
// Usage: tickloop T N [CPULIST]   (CPULIST like 2-7 or 2,3,4; "-" leaves affinity alone)
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

__attribute__((noinline)) unsigned long tick(unsigned long x)
{
	unsigned long s = x;
	for (int i = 0; i < 4; i++)
		s = s * 2654435761u + i;
	__asm__ volatile("" : "+r"(s)::"memory");
	return s;
}

struct worker {
	unsigned long n, acc;
	char pad[48];
} __attribute__((aligned(64)));

static pthread_barrier_t barrier;

static void *run(void *arg)
{
	struct worker *w = arg;
	unsigned long acc = 0;
	pthread_barrier_wait(&barrier);
	for (unsigned long i = 0; i < w->n; i++)
		acc += tick(i);
	w->acc = acc;
	return NULL;
}

static int parse_cpus(const char *s, cpu_set_t *set)
{
	CPU_ZERO(set);
	while (*s) {
		char *e;
		long a = strtol(s, &e, 10), b = a;
		if (e == s)
			return -1;
		if (*e == '-')
			b = strtol(e + 1, &e, 10);
		for (long c = a; c <= b; c++)
			CPU_SET(c, set);
		s = *e == ',' ? e + 1 : e;
		if (*e && *e != ',')
			return -1;
	}
	return 0;
}

static double now(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: tickloop T N [CPULIST]\n");
		return 1;
	}
	int T = atoi(argv[1]);
	unsigned long N = strtoul(argv[2], NULL, 10);
	const char *cpus = argc > 3 ? argv[3] : "2-7";
	if (T < 1 || T > 1024 || N < 1) {
		fprintf(stderr, "bad T or N\n");
		return 1;
	}
	if (strcmp(cpus, "-")) {
		cpu_set_t set;
		if (parse_cpus(cpus, &set) || sched_setaffinity(0, sizeof(set), &set)) {
			perror("cpu list");
			return 1;
		}
	}
	struct worker *ws = aligned_alloc(64, sizeof(*ws) * T);
	memset(ws, 0, sizeof(*ws) * T);
	pthread_t *tids = calloc(T, sizeof(*tids));
	pthread_barrier_init(&barrier, NULL, T + 1);
	for (int t = 0; t < T; t++) {
		ws[t].n = N;
		if (pthread_create(&tids[t], NULL, run, &ws[t])) {
			perror("pthread_create");
			return 1;
		}
	}
	pthread_barrier_wait(&barrier);
	double t0 = now();
	unsigned long acc = 0;
	for (int t = 0; t < T; t++) {
		pthread_join(tids[t], NULL);
		acc += ws[t].acc;
	}
	double wall = now() - t0;
	printf("{\"T\":%d,\"N\":%lu,\"cpus\":\"%s\",\"wall_s\":%.4f,\"expected\":%lu,"
	       "\"acc\":%lu}\n",
	       T, N, cpus, wall, (unsigned long)T * N, acc & 0xffff);
	return 0;
}

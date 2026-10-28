// Target for the shared-counter benchmark: THREADS threads (thread i pinned to CPU 5 - i, or to
// the i-th CPU of CPUS), each calling count_event(key) in a loop for SECS seconds with keys
// cycling over 0..NKEYS-1, so all threads update the same NKEYS counters. Prints the median
// calls/s and the exact number of calls.
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct counter {
	_Atomic unsigned long calls;
	unsigned long sink;
	char pad[48];
} __attribute__((aligned(64)));

static struct counter counters[6];
static int cpu_of[6] = { 5, 4, 3, 2, 1, 0 };
static atomic_int stop;
static unsigned nkeys;

__attribute__((noinline)) unsigned long count_event(unsigned long key)
{
	__asm__ volatile("" : : "r"(key) : "memory");
	return key * 2654435761u;
}

static double now(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec / 1e9;
}

static int cmp(const void *a, const void *b)
{
	double x = *(const double *)a, y = *(const double *)b;
	return (x > y) - (x < y);
}

static void *worker(void *arg)
{
	long id = (long)arg;
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(cpu_of[id], &set);
	pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
	unsigned long local = 0, acc = 0, key = id % nkeys;
	while (!atomic_load_explicit(&stop, memory_order_relaxed)) {
		for (int j = 0; j < 100; j++) {
			acc += count_event(key);
			if (++key == nkeys)
				key = 0;
		}
		local += 100;
		atomic_store_explicit(&counters[id].calls, local, memory_order_relaxed);
	}
	*(volatile unsigned long *)&counters[id].sink = acc;
	return NULL;
}

int main(int argc, char **argv)
{
	int threads = argc > 1 ? atoi(argv[1]) : 1;
	int secs = argc > 2 ? atoi(argv[2]) : 10;
	nkeys = argc > 3 ? (unsigned)atoi(argv[3]) : 1;
	if (argc > 4) {
		char *p = argv[4];
		for (int t = 0; t < 6 && *p; t++) {
			cpu_of[t] = (int)strtol(p, &p, 10);
			if (*p == ',')
				p++;
		}
	}
	if (threads < 1 || threads > 6 || secs < 1 || nkeys < 1 || nkeys > 1024) {
		fprintf(stderr, "usage: countloop THREADS(1-6) SECS NKEYS(1-1024) [CPUS]\n");
		return 1;
	}
	pthread_t tids[6];
	for (long t = 0; t < threads; t++)
		pthread_create(&tids[t], NULL, worker, (void *)t);
	double *rates = calloc(secs, sizeof(double));
	unsigned long last = 0;
	double mark = now();
	for (int k = 0; k < secs; k++) {
		struct timespec one = { 1, 0 };
		nanosleep(&one, NULL);
		unsigned long sum = 0;
		for (int t = 0; t < threads; t++)
			sum += atomic_load_explicit(&counters[t].calls, memory_order_relaxed);
		double t = now();
		rates[k] = (sum - last) / (t - mark);
		last = sum;
		mark = t;
	}
	atomic_store(&stop, 1);
	unsigned long total = 0;
	for (int t = 0; t < threads; t++) {
		pthread_join(tids[t], NULL);
		total += atomic_load(&counters[t].calls);
	}
	qsort(rates, secs, sizeof(double), cmp);
	printf("threads %d nkeys %u\n", threads, nkeys);
	printf("ops_per_sec_median %.1f\n", rates[secs / 2]);
	printf("total_ops %lu\n", total);
	return 0;
}

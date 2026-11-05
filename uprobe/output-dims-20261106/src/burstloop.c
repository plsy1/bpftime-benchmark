// Bursty target for the output benchmark: THREADS threads (thread i pinned to CPU 5 - i), each
// calling count_event(key) BURST times, GAP_NS of work apart (0: back to back), then sleeping
// PAUSE_US, for SECS seconds. With a gap the arrival rate is the workload's, not the probe's.
// Prints the exact number of calls and the median time of a burst (us).
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int cpu_of[6] = { 5, 4, 3, 2, 1, 0 };
static atomic_int stop;
static int burst, pause_us;
static long gap_ns;
static _Atomic unsigned long total;
static double burst_us[6];

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
	unsigned long calls = 0, acc = 0;
	static __thread double samples[1 << 16];
	int n = 0;
	struct timespec p = { pause_us / 1000000, (pause_us % 1000000) * 1000L };
	while (!atomic_load_explicit(&stop, memory_order_relaxed)) {
		double t0 = now();
		for (int j = 0; j < burst; j++) {
			acc += count_event(calls + j);
			if (gap_ns > 0) {
				double until = now() + gap_ns / 1e9;
				while (now() < until)
					;
			}
		}
		double t1 = now();
		calls += burst;
		if (n < (1 << 16))
			samples[n++] = (t1 - t0) * 1e6;
		nanosleep(&p, NULL);
	}
	qsort(samples, n, sizeof(double), cmp);
	burst_us[id] = n ? samples[n / 2] : 0;
	atomic_fetch_add(&total, calls + (acc & 0));
	return NULL;
}

int main(int argc, char **argv)
{
	int threads = argc > 1 ? atoi(argv[1]) : 1;
	int secs = argc > 2 ? atoi(argv[2]) : 5;
	burst = argc > 3 ? atoi(argv[3]) : 100;
	pause_us = argc > 4 ? atoi(argv[4]) : 10000;
	gap_ns = argc > 5 ? atol(argv[5]) : 0;
	if (threads < 1 || threads > 6 || secs < 1 || burst < 1 || pause_us < 0) {
		fprintf(stderr, "usage: burstloop THREADS(1-6) SECS BURST PAUSE_US [GAP_NS]\n");
		return 1;
	}
	pthread_t t[6];
	for (long i = 0; i < threads; i++)
		pthread_create(&t[i], NULL, worker, (void *)i);
	struct timespec s = { secs, 0 };
	nanosleep(&s, NULL);
	atomic_store(&stop, 1);
	for (int i = 0; i < threads; i++)
		pthread_join(t[i], NULL);
	qsort(burst_us, threads, sizeof(double), cmp);
	printf("total_ops %lu\nburst_us_median %.1f\nops_per_sec_median %.1f\n",
	       atomic_load(&total), burst_us[threads / 2], atomic_load(&total) / (double)secs);
	return 0;
}

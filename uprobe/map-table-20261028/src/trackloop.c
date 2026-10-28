// memleak-like target for the tracking-table benchmark: THREADS threads (thread i pinned to CPU
// 5 - i, or to the i-th CPU of CPUS), each owning LIVE/THREADS outstanding pseudo-random 64-bit
// "addresses". Prefill: alloc_event(addr) for each. Then for SECS seconds: free_event(oldest),
// alloc_event(new), so the traced table stays at LIVE entries. One op = one free + one alloc.
// Prints the median ops/s, the exact op count and the exact number of allocs and frees.
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct counter {
	_Atomic unsigned long ops;
	unsigned long allocs, frees, sink;
	char pad[32];
} __attribute__((aligned(64)));

static struct counter counters[6];
static int cpu_of[6] = { 5, 4, 3, 2, 1, 0 };
static atomic_int started, go, stop;
static long live_per_thread;

__attribute__((noinline)) unsigned long alloc_event(unsigned long addr)
{
	__asm__ volatile("" : : "r"(addr) : "memory");
	return addr ^ 0x5bd1e995;
}

__attribute__((noinline)) unsigned long free_event(unsigned long addr)
{
	__asm__ volatile("" : : "r"(addr) : "memory");
	return addr + 0x9e3779b9;
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
	unsigned long *ring = malloc(sizeof(unsigned long) * live_per_thread);
	// Unique scattered addresses: a per-thread counter (thread id in the top bits) times an odd
	// constant modulo 2^44 is a bijection, so no address repeats; shifted to look like 16-byte
	// aligned user pointers. (Random addresses repeated about once per run at 100K+ live entries:
	// a repeated insert overwrote, and its second free missed.)
	unsigned long seq = (unsigned long)id << 38, acc = 0, allocs = 0, frees = 0;
#define NEXT() ((((seq++) * 0x9e3779b97f4a7c15ul) & 0x00000ffffffffffful) << 4 | 0x10)
	for (long i = 0; i < live_per_thread; i++) {
		ring[i] = NEXT();
		acc += alloc_event(ring[i]);
		allocs++;
	}
	atomic_fetch_add(&started, 1);
	while (!atomic_load(&go))
		;
	unsigned long ops = 0;
	long idx = 0;
	while (!atomic_load_explicit(&stop, memory_order_relaxed)) {
		for (int j = 0; j < 100; j++) {
			acc += free_event(ring[idx]);
			ring[idx] = NEXT();
			acc += alloc_event(ring[idx]);
			if (++idx == live_per_thread)
				idx = 0;
		}
		ops += 100;
		frees += 100;
		allocs += 100;
		atomic_store_explicit(&counters[id].ops, ops, memory_order_relaxed);
	}
	counters[id].allocs = allocs;
	counters[id].frees = frees;
	*(volatile unsigned long *)&counters[id].sink = acc;
	free(ring);
	return NULL;
}

int main(int argc, char **argv)
{
	int threads = argc > 1 ? atoi(argv[1]) : 1;
	int secs = argc > 2 ? atoi(argv[2]) : 10;
	long live = argc > 3 ? atol(argv[3]) : 1000;
	if (argc > 4) {
		char *p = argv[4];
		for (int t = 0; t < 6 && *p; t++) {
			cpu_of[t] = (int)strtol(p, &p, 10);
			if (*p == ',')
				p++;
		}
	}
	if (threads < 1 || threads > 6 || secs < 1 || live < threads) {
		fprintf(stderr, "usage: trackloop THREADS(1-6) SECS LIVE [CPUS]\n");
		return 1;
	}
	live_per_thread = live / threads;
	pthread_t tids[6];
	for (long t = 0; t < threads; t++)
		pthread_create(&tids[t], NULL, worker, (void *)t);
	while (atomic_load(&started) < threads) {
		struct timespec ms = { 0, 1000000 };
		nanosleep(&ms, NULL);
	}
	atomic_store(&go, 1);
	double *rates = calloc(secs, sizeof(double));
	unsigned long last = 0;
	double mark = now();
	for (int k = 0; k < secs; k++) {
		struct timespec one = { 1, 0 };
		nanosleep(&one, NULL);
		unsigned long sum = 0;
		for (int t = 0; t < threads; t++)
			sum += atomic_load_explicit(&counters[t].ops, memory_order_relaxed);
		double t = now();
		rates[k] = (sum - last) / (t - mark);
		last = sum;
		mark = t;
	}
	atomic_store(&stop, 1);
	unsigned long total = 0, allocs = 0, frees = 0;
	for (int t = 0; t < threads; t++) {
		pthread_join(tids[t], NULL);
		total += atomic_load(&counters[t].ops);
		allocs += counters[t].allocs;
		frees += counters[t].frees;
	}
	qsort(rates, secs, sizeof(double), cmp);
	printf("threads %d live %ld\n", threads, live_per_thread * threads);
	printf("ops_per_sec_median %.1f\n", rates[secs / 2]);
	printf("total_ops %lu\n", total);
	printf("allocs %lu\nfrees %lu\n", allocs, frees);
	return 0;
}

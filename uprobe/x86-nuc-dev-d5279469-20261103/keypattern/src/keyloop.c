// Tracking-table churn with chosen key patterns: THREADS threads (thread i pinned to CPU 5 - i),
// each owning LIVE/THREADS live keys. Prefill: insert_event(key) for each. Then for SECS
// seconds: delete_event(oldest), insert_event(new), so the table stays at LIVE entries. Keys come
// from per-thread ids taken modulo 2 x the thread's live count (FIFO, so the live ids are always
// distinct) interleaved over threads, through PATTERN:
//   addr    scattered 16-byte aligned 64-bit addresses (a bijection of the id)
//   seq64   tgid << 32 | id (pid_tgid-like)
//   rand32  a bijection of the id on 32 bits
//   shl12   id << 12 on 32 bits (a packed field above fixed low bits)
// One op = one delete + one insert. Prints the median ops/s, total ops, inserts and deletes.
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct counter {
	_Atomic unsigned long ops;
	unsigned long inserts, deletes, sink;
	char pad[32];
} __attribute__((aligned(64)));

static struct counter counters[6];
static int cpu_of[6] = { 5, 4, 3, 2, 1, 0 };
static atomic_int started, go, stop;
static long live_per_thread, threads;
static int pattern;

__attribute__((noinline)) unsigned long insert_event(unsigned long key)
{
	__asm__ volatile("" : : "r"(key) : "memory");
	return key ^ 0x5bd1e995;
}

__attribute__((noinline)) unsigned long delete_event(unsigned long key)
{
	__asm__ volatile("" : : "r"(key) : "memory");
	return key + 0x9e3779b9;
}

static unsigned long make_key(unsigned long gid)
{
	switch (pattern) {
	case 0:
		return ((gid * 0x9e3779b97f4a7c15ul) & 0x00000ffffffffffful) << 4 | 0x10;
	case 1:
		return (4242ul << 32) | (unsigned)gid;
	case 2: {
		unsigned k = (unsigned)gid * 2654435761u;
		return k ^ (k >> 16);
	}
	default:
		return (unsigned)(gid << 12);
	}
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
	const unsigned long space = 2 * (unsigned long)live_per_thread;
	unsigned long seq = 0, acc = 0, inserts = 0, deletes = 0;
#define NEXT() make_key((seq++ % space) * threads + id)
	for (long i = 0; i < live_per_thread; i++) {
		ring[i] = NEXT();
		acc += insert_event(ring[i]);
		inserts++;
	}
	atomic_fetch_add(&started, 1);
	while (!atomic_load(&go))
		;
	unsigned long ops = 0;
	long idx = 0;
	while (!atomic_load_explicit(&stop, memory_order_relaxed)) {
		for (int j = 0; j < 100; j++) {
			acc += delete_event(ring[idx]);
			ring[idx] = NEXT();
			acc += insert_event(ring[idx]);
			if (++idx == live_per_thread)
				idx = 0;
		}
		ops += 100;
		deletes += 100;
		inserts += 100;
		atomic_store_explicit(&counters[id].ops, ops, memory_order_relaxed);
	}
	counters[id].inserts = inserts;
	counters[id].deletes = deletes;
	*(volatile unsigned long *)&counters[id].sink = acc;
	free(ring);
	return NULL;
}

int main(int argc, char **argv)
{
	threads = argc > 1 ? atoi(argv[1]) : 1;
	int secs = argc > 2 ? atoi(argv[2]) : 5;
	long live = argc > 3 ? atol(argv[3]) : 1000;
	const char *names[] = { "addr", "seq64", "rand32", "shl12" };
	pattern = -1;
	for (int i = 0; i < 4; i++)
		if (argc > 4 && strcmp(argv[4], names[i]) == 0)
			pattern = i;
	if (threads < 1 || threads > 6 || secs < 1 || live < threads || pattern < 0) {
		fprintf(stderr, "usage: keyloop THREADS(1-6) SECS LIVE addr|seq64|rand32|shl12\n");
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
	unsigned long total = 0, inserts = 0, deletes = 0;
	for (long t = 0; t < threads; t++) {
		pthread_join(tids[t], NULL);
		total += atomic_load(&counters[t].ops);
		inserts += counters[t].inserts;
		deletes += counters[t].deletes;
	}
	qsort(rates, secs, sizeof(double), cmp);
	printf("ops_per_sec_median %.1f\ntotal_ops %lu\nallocs %lu\nfrees %lu\n", rates[secs / 2], total,
	       inserts, deletes);
	return 0;
}

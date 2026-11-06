// Bursty target for the output benchmark: THREADS threads (thread i pinned to CPU 5 - i), each
// calling count_event(key) BURST times, GAP_NS of work apart (0: back to back), then sleeping
// PAUSE_US, for SECS seconds. With a gap the arrival rate is the workload's, not the probe's.
// PERIOD_NS > 0 instead paces each burst by the clock: call j at the burst's start + j * PERIOD,
// so the arrival rate is the same whatever the probe costs (as long as it costs less). Every
// call passes its CLOCK_MONOTONIC send time (ns) as the key: the consumer measures delivery.
// Prints the exact number of calls and the median time of a burst (us).
// Environment (more-dims-20261106 additions, all off by default):
//   CPUS=a,b,...  CPU of thread i (default 5,4,3,2,1,0).
//   FLUSH=1       after the run each thread waits 50 ms, then makes 3 calls 1 ms apart from
//                 the same thread (a lost-sample count still pending in its perf ring or shard is
//                 reported only with a later sample that fits); they are counted.
//   CALL_HIST=1   time every count_event call (clock read before and after) into a log-scale
//                 histogram (8 steps per power of two, 12.5%); prints call_p50/p90/p99/p999/max
//                 in ns over all threads.
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int cpu_of[6] = { 5, 4, 3, 2, 1, 0 };
static int flush, call_hist;
#define HBUCKETS 512
static _Atomic unsigned long hist[HBUCKETS];
static _Atomic unsigned long hist_max;

static int bucket_of(unsigned long d)
{
	if (d < 8)
		return (int)d;
	int msb = 63 - __builtin_clzl(d);
	int b = msb * 8 + (int)((d >> (msb - 3)) & 7);
	return b < HBUCKETS ? b : HBUCKETS - 1;
}
static unsigned long bucket_low(int b)
{
	if (b < 8)
		return (unsigned long)b;
	int msb = b / 8;
	return (1UL << msb) | ((unsigned long)(b % 8) << (msb - 3));
}
static atomic_int stop;
static int burst, pause_us;
static long gap_ns, period_ns;

static unsigned long mono_ns(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000UL + t.tv_nsec;
}
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
	static __thread unsigned long lh[HBUCKETS];
	unsigned long lmax = 0;
	int n = 0;
	struct timespec p = { pause_us / 1000000, (pause_us % 1000000) * 1000L };
	while (!atomic_load_explicit(&stop, memory_order_relaxed)) {
		double t0 = now();
		unsigned long start = mono_ns();
		for (int j = 0; j < burst; j++) {
			if (period_ns > 0)
				while (mono_ns() < start + (unsigned long)j * period_ns)
					;
			if (call_hist) {
				unsigned long a = mono_ns();
				acc += count_event(a);
				unsigned long d = mono_ns() - a;
				lh[bucket_of(d)]++;
				if (d > lmax)
					lmax = d;
			} else {
				acc += count_event(mono_ns());
			}
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
	if (flush) {
		struct timespec w = { 0, 50 * 1000000L }, g = { 0, 1000000L };
		nanosleep(&w, NULL);
		for (int j = 0; j < 3; j++) {
			acc += count_event(mono_ns());
			calls++;
			nanosleep(&g, NULL);
		}
	}
	if (call_hist) {
		for (int b = 0; b < HBUCKETS; b++)
			if (lh[b])
				atomic_fetch_add(&hist[b], lh[b]);
		unsigned long m = atomic_load(&hist_max);
		while (lmax > m && !atomic_compare_exchange_weak(&hist_max, &m, lmax))
			;
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
	period_ns = argc > 6 ? atol(argv[6]) : 0;
	if (threads < 1 || threads > 6 || secs < 1 || burst < 1 || pause_us < 0) {
		fprintf(stderr, "usage: burstloop THREADS(1-6) SECS BURST PAUSE_US [GAP_NS [PERIOD_NS]]\n");
		return 1;
	}
	const char *cpus = getenv("CPUS");
	for (int i = 0; cpus && *cpus && i < 6; i++) {
		cpu_of[i] = atoi(cpus);
		cpus = strchr(cpus, ',');
		if (!cpus)
			break;
		cpus++;
	}
	flush = getenv("FLUSH") && atoi(getenv("FLUSH"));
	call_hist = getenv("CALL_HIST") && atoi(getenv("CALL_HIST"));
	pthread_t t[6];
	for (long i = 0; i < threads; i++)
		pthread_create(&t[i], NULL, worker, (void *)i);
	struct timespec s = { secs, 0 };
	nanosleep(&s, NULL);
	atomic_store(&stop, 1);
	for (int i = 0; i < threads; i++)
		pthread_join(t[i], NULL);
	qsort(burst_us, threads, sizeof(double), cmp);
	if (call_hist) {
		unsigned long n = 0;
		for (int b = 0; b < HBUCKETS; b++)
			n += hist[b];
		static const int per[] = { 500, 900, 990, 999 };
		static const char *name[] = { "p50", "p90", "p99", "p999" };
		for (int k = 0; k < 4; k++) {
			unsigned long want = (n * per[k] + 999) / 1000, seen = 0;
			int b = 0;
			for (; b < HBUCKETS; b++) {
				seen += hist[b];
				if (seen >= want)
					break;
			}
			printf("call_%s %lu\n", name[k], bucket_low(b));
		}
		printf("call_max %lu\ncall_n %lu\n", atomic_load(&hist_max), n);
	}
	printf("total_ops %lu\nburst_us_median %.1f\nops_per_sec_median %.1f\n",
	       atomic_load(&total), burst_us[threads / 2], atomic_load(&total) / (double)secs);
	return 0;
}

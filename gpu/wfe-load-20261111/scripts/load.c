// CPU load for the WFE test: load THREADS OUT_FILE
// THREADS unpinned workers each run a xorshift loop and add the iterations to
// their own counter (own cache line). A sampler thread writes
// "<CLOCK_MONOTONIC ns> <sum of counters>" every 10 ms to OUT_FILE.
// Stops on SIGINT/SIGTERM.
#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAXT 64
struct counter {
	_Alignas(64) atomic_uint_fast64_t n;
};
static struct counter counters[MAXT];
static atomic_int stop;

static void on_sig(int s)
{
	(void)s;
	atomic_store(&stop, 1);
}

static void *worker(void *arg)
{
	struct counter *c = arg;
	uint64_t x = 88172645463325252ull ^ (uintptr_t)arg;
	while (!atomic_load_explicit(&stop, memory_order_relaxed)) {
		for (int i = 0; i < 4096; i++) {
			x ^= x << 13;
			x ^= x >> 7;
			x ^= x << 17;
		}
		atomic_fetch_add_explicit(&c->n, 1 + (x & 0), memory_order_relaxed);
	}
	return (void *)(uintptr_t)x;
}

static long long mono_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: load THREADS OUT_FILE\n");
		return 2;
	}
	int n = atoi(argv[1]);
	if (n < 1 || n > MAXT)
		return 2;
	FILE *f = fopen(argv[2], "w");
	if (!f)
		return 1;
	signal(SIGINT, on_sig);
	signal(SIGTERM, on_sig);
	pthread_t th[MAXT];
	for (int i = 0; i < n; i++)
		pthread_create(&th[i], NULL, worker, &counters[i]);
	struct timespec d = { 0, 10 * 1000 * 1000 };
	while (!atomic_load(&stop)) {
		nanosleep(&d, NULL);
		uint64_t sum = 0;
		for (int i = 0; i < n; i++)
			sum += atomic_load_explicit(&counters[i].n,
						    memory_order_relaxed);
		fprintf(f, "%lld %llu\n", mono_ns(), (unsigned long long)sum);
	}
	for (int i = 0; i < n; i++)
		pthread_join(th[i], NULL);
	fclose(f);
	return 0;
}

// Target for the torn-read test: THREADS threads (thread i pinned to CPU 5 - i, or to the i-th
// CPU of CPUS); thread 0 calls write_event(seq) with an increasing seq, the others call
// read_event(), for SECS seconds. Prints the exact numbers of writes and reads.
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

__attribute__((noinline)) unsigned long write_event(unsigned long seq)
{
	__asm__ volatile("" : : "r"(seq) : "memory");
	return seq * 3;
}

__attribute__((noinline)) unsigned long read_event(void)
{
	__asm__ volatile("" : : : "memory");
	return 7;
}

static void *worker(void *arg)
{
	long id = (long)arg;
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(cpu_of[id], &set);
	pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
	unsigned long local = 0, acc = 0, seq = 1;
	while (!atomic_load_explicit(&stop, memory_order_relaxed)) {
		for (int j = 0; j < 100; j++)
			acc += id == 0 ? write_event(seq++) : read_event();
		local += 100;
		atomic_store_explicit(&counters[id].calls, local, memory_order_relaxed);
	}
	*(volatile unsigned long *)&counters[id].sink = acc;
	return NULL;
}

int main(int argc, char **argv)
{
	int threads = argc > 1 ? atoi(argv[1]) : 2;
	int secs = argc > 2 ? atoi(argv[2]) : 5;
	if (argc > 3) {
		char *p = argv[3];
		for (int t = 0; t < 6 && *p; t++) {
			cpu_of[t] = (int)strtol(p, &p, 10);
			if (*p == ',')
				p++;
		}
	}
	if (threads < 2 || threads > 6 || secs < 1) {
		fprintf(stderr, "usage: tornloop THREADS(2-6) SECS [CPUS]\n");
		return 1;
	}
	pthread_t tids[6];
	for (long t = 0; t < threads; t++)
		pthread_create(&tids[t], NULL, worker, (void *)t);
	struct timespec ts = { secs, 0 };
	nanosleep(&ts, NULL);
	atomic_store(&stop, 1);
	unsigned long reads = 0;
	for (int t = 0; t < threads; t++) {
		pthread_join(tids[t], NULL);
		if (t)
			reads += atomic_load(&counters[t].calls);
	}
	printf("threads %d\nwrites %lu\nreads %lu\n", threads, atomic_load(&counters[0].calls), reads);
	return 0;
}

// Footprint target (more-dims-20261106/n5-footprint): THREADS threads each call count_event once
// and wait; then main reads its own /proc/self/status (VmRSS, RssAnon, RssFile, RssShmem,
// Threads) and prints it, sleeps HOLD seconds with every thread blocked, prints its own CPU time
// (getrusage, all threads: the agent's background work while idle) and exits.
// Usage: thr THREADS HOLD
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

__attribute__((noinline)) unsigned long count_event(unsigned long key)
{
	unsigned long h = key;
	for (int i = 0; i < 4; i++)
		h = h * 2654435761u + i;
	__asm__ volatile("" : : "r"(h) : "memory");
	return h;
}

static pthread_barrier_t ready, done;

static void *worker(void *arg)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	count_event(t.tv_sec * 1000000000UL + t.tv_nsec + (unsigned long)arg);
	pthread_barrier_wait(&ready);
	pthread_barrier_wait(&done);
	return NULL;
}

static void status(void)
{
	FILE *f = fopen("/proc/self/status", "r");
	char line[256];
	static const char *keys[] = { "VmRSS:", "RssAnon:", "RssFile:", "RssShmem:", "Threads:" };
	while (f && fgets(line, sizeof(line), f))
		for (int k = 0; k < 5; k++)
			if (!strncmp(line, keys[k], strlen(keys[k])))
				printf("%.*s %ld\n", (int)strlen(keys[k]) - 1, keys[k],
				       atol(line + strlen(keys[k])));
	if (f)
		fclose(f);
}

int main(int argc, char **argv)
{
	int n = argc > 1 ? atoi(argv[1]) : 1, hold = argc > 2 ? atoi(argv[2]) : 0;
	pthread_t *t = calloc(n, sizeof(*t));
	pthread_barrier_init(&ready, NULL, n + 1);
	pthread_barrier_init(&done, NULL, n + 1);
	for (long i = 0; i < n; i++)
		pthread_create(&t[i], NULL, worker, (void *)i);
	pthread_barrier_wait(&ready);
	status();
	fflush(stdout);
	struct rusage r0, r1;
	getrusage(RUSAGE_SELF, &r0);
	sleep(hold);
	getrusage(RUSAGE_SELF, &r1);
	double cpu = (r1.ru_utime.tv_sec - r0.ru_utime.tv_sec) + (r1.ru_stime.tv_sec - r0.ru_stime.tv_sec) +
		     ((r1.ru_utime.tv_usec - r0.ru_utime.tv_usec) + (r1.ru_stime.tv_usec - r0.ru_stime.tv_usec)) / 1e6;
	printf("idle_cpu_ms %.1f\nidle_secs %d\ncalls %d\n", cpu * 1000, hold, n);
	pthread_barrier_wait(&done);
	for (int i = 0; i < n; i++)
		pthread_join(t[i], NULL);
	return 0;
}

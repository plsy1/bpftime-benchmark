// gethostlatency target (output-dims-20261106/r8): bursts of BURST getaddrinfo calls, call j at
// the burst's start + j * PERIOD_US, every PAUSE_US, for SECS seconds, on CPU 5. Call i resolves
// the numeric host "127.a.b.c" with a.b.c = i + 1 (unique, no lookup in /etc/hosts or DNS), so
// each line of the tool names its call. Writes "i send_ns" (CLOCK_MONOTONIC when the call
// started) to SENDLOG and prints total_ops.
#define _GNU_SOURCE
#include <netdb.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static unsigned long mono_ns(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000UL + t.tv_nsec;
}

int main(int argc, char **argv)
{
	if (argc < 6) {
		fprintf(stderr, "usage: resolveburst SECS BURST PERIOD_US PAUSE_US SENDLOG\n");
		return 1;
	}
	int secs = atoi(argv[1]), burst = atoi(argv[2]);
	long period = atol(argv[3]) * 1000, pause_us = atol(argv[4]);
	FILE *log = fopen(argv[5], "w");
	if (!log)
		return 1;
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(5, &set);
	sched_setaffinity(0, sizeof(set), &set);
	size_t cap = 1 << 22, n = 0;
	unsigned long *sent = malloc(cap * sizeof(*sent));
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_flags = AI_NUMERICHOST;
	unsigned long end = mono_ns() + secs * 1000000000UL;
	struct timespec p = { pause_us / 1000000, (pause_us % 1000000) * 1000 };
	while (mono_ns() < end && n + burst < cap) {
		unsigned long start = mono_ns();
		for (int j = 0; j < burst; j++) {
			while (mono_ns() < start + (unsigned long)j * period)
				;
			unsigned long id = n + 1;
			char host[32];
			snprintf(host, sizeof(host), "127.%lu.%lu.%lu", (id >> 16) & 255, (id >> 8) & 255,
				 id & 255);
			struct addrinfo *res = NULL;
			sent[n++] = mono_ns();
			if (getaddrinfo(host, NULL, &hints, &res) == 0)
				freeaddrinfo(res);
		}
		nanosleep(&p, NULL);
	}
	for (size_t i = 0; i < n; i++)
		fprintf(log, "%zu %lu\n", i + 1, sent[i]);
	fclose(log);
	printf("total_ops %zu\n", n);
	return 0;
}

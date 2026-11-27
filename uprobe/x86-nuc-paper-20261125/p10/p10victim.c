// P10 victim: calls the noinline p10_point(i) WARM times untimed, then N times timed (CLOCK_MONOTONIC), prints
// "ns_per_call <x> calls <N>". Usage: p10victim N WARM
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Body of a few real instructions: Frida cannot hook a function that is only `ret` (GUM_ATTACH_WRONG_SIGNATURE).
static volatile unsigned long sink;
__attribute__((noinline)) void p10_point(unsigned long i)
{
	sink = i;
	sink = sink + 1;
	__asm__ volatile("" : : "r"(i) : "memory");
}

static double now(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1e9 + t.tv_nsec;
}

int main(int argc, char **argv)
{
	unsigned long n = argc > 1 ? strtoul(argv[1], NULL, 10) : 1000000;
	unsigned long w = argc > 2 ? strtoul(argv[2], NULL, 10) : 100000;
	for (unsigned long i = 0; i < w; i++)
		p10_point(i);
	double t0 = now();
	for (unsigned long i = 0; i < n; i++)
		p10_point(w + i);
	double t1 = now();
	printf("ns_per_call %.2f calls %lu\n", (t1 - t0) / n, n);
	return 0;
}

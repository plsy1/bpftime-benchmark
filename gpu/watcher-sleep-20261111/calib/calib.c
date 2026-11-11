// Calibration of short sleeps: calib SLACK_NS PERIOD_US...
// For each period: one thread sets its timer slack, loops clock_nanosleep(period) for 2 s,
// records each real interval, and reports the mean / median / p99 interval (us), wake-ups per
// second and the thread's CPU per wake-up (us, RUSAGE_THREAD) and CPU share.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <time.h>

static long long ns(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000LL + t.tv_nsec;
}
static double tcpu(void)
{
	struct rusage r;
	getrusage(RUSAGE_THREAD, &r);
	return r.ru_utime.tv_sec + r.ru_utime.tv_usec / 1e6 + r.ru_stime.tv_sec +
	       r.ru_stime.tv_usec / 1e6;
}
static int cmp(const void *a, const void *b)
{
	double x = *(const double *)a, y = *(const double *)b;
	return x < y ? -1 : x > y;
}

int main(int argc, char **argv)
{
	if (argc < 3)
		return 2;
	unsigned long slack = strtoul(argv[1], NULL, 10);
	if (prctl(PR_SET_TIMERSLACK, slack) != 0)
		perror("prctl");
	static double iv[400000];
	for (int a = 2; a < argc; a++) {
		long p = atol(argv[a]);
		struct timespec d = { 0, p * 1000 };
		int n = 0;
		double c0 = tcpu();
		long long s = ns(), prev = s;
		while (prev - s < 2000000000LL && n < 400000) {
			clock_nanosleep(CLOCK_MONOTONIC, 0, &d, NULL);
			long long t = ns();
			iv[n++] = (t - prev) / 1000.0;
			prev = t;
		}
		double c = tcpu() - c0, wall = (prev - s) / 1e9, sum = 0;
		for (int i = 0; i < n; i++)
			sum += iv[i];
		qsort(iv, n, sizeof(double), cmp);
		printf("slack_ns=%lu period_us=%ld wakeups=%d per_s=%.0f interval_us mean=%.1f median=%.1f p99=%.1f max=%.1f cpu_per_wake_us=%.2f cpu_share=%.3f\n",
		       slack, p, n, n / wall, sum / n, iv[n / 2], iv[(int)(n * 0.99)], iv[n - 1],
		       c * 1e6 / n, c / wall);
		fflush(stdout);
	}
	return 0;
}

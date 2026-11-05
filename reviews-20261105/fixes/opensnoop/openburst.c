// openloop with a configurable burst: `burst` open+close pairs back to back,
// then usleep(pause_us), for `duration` seconds. Prints the total and the
// median time one burst took, so a burst can be compared with the consumer's
// polling interval.
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static double now(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

static int cmp(const void *a, const void *b)
{
	double x = *(const double *)a, y = *(const double *)b;
	return x < y ? -1 : x > y;
}

int main(int argc, char **argv)
{
	int duration = argc > 1 ? atoi(argv[1]) : 4;
	int burst = argc > 2 ? atoi(argv[2]) : 100;
	int pause_us = argc > 3 ? atoi(argv[3]) : 10000;
	const char *path = argc > 4 ? argv[4] : "/etc/hostname";
	static double bursts[1 << 20];
	unsigned long total = 0, n = 0;
	double start = now();
	while (now() - start < duration) {
		double t0 = now();
		for (int j = 0; j < burst; j++) {
			int fd = open(path, O_RDONLY);
			if (fd < 0) {
				perror("open");
				return 1;
			}
			close(fd);
		}
		if (n < sizeof(bursts) / sizeof(bursts[0]))
			bursts[n++] = (now() - t0) * 1e6;
		total += burst;
		if (pause_us)
			usleep(pause_us);
	}
	double secs = now() - start;
	qsort(bursts, n, sizeof(double), cmp);
	printf("burst %d pause_us %d bursts %lu burst_us_median %.1f\n", burst,
	       pause_us, n, bursts[n / 2]);
	printf("ops_per_sec %.1f\n", total / secs);
	printf("total_ops %lu\n", total);
	return 0;
}

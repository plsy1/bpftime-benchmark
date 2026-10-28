// WRITES write(2) calls of 7 bytes on fd 42 (/dev/null); prints the count and
// the time per call.
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	long n = argc > 1 ? atol(argv[1]) : 1000000;
	int fd = open("/dev/null", O_WRONLY);
	if (fd < 0 || dup2(fd, 42) != 42)
		return 1;
	struct timespec a, b;
	clock_gettime(CLOCK_MONOTONIC, &a);
	for (long i = 0; i < n; i++)
		if (write(42, "bpftime", 7) != 7)
			return 2;
	clock_gettime(CLOCK_MONOTONIC, &b);
	double ns = (b.tv_sec - a.tv_sec) * 1e9 + (b.tv_nsec - a.tv_nsec);
	printf("writes %ld\nns_per_write %.1f\n", n, ns / n);
	return 0;
}

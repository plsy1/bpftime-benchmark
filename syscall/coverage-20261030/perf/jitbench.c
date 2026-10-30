// N distinct JIT call sites (mov x8, #getppid; svc #0; ret): round 1 runs each
// once (in lazy mode: a SIGSYS trap and the rewrite), round 2 runs them again.
// Also times the mprotect that makes them executable (rescan with anonymous
// scanning on). Prints ns per call.
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
static double now(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1e9 + ts.tv_nsec;
}
int main(int argc, char **argv)
{
	int n = argc > 1 ? atoi(argv[1]) : 1000;
	size_t size = ((size_t)n * 16 + 4095) & ~4095ul;
	uint32_t *code = mmap(NULL, size, PROT_READ | PROT_WRITE,
			      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	for (int i = 0; i < n; i++) {
		code[4 * i] = 0xd2800008 | (SYS_getppid << 5);
		code[4 * i + 1] = 0xd4000001;
		code[4 * i + 2] = 0xd65f03c0;
		code[4 * i + 3] = 0xd503201f;
	}
	__builtin___clear_cache((char *)code, (char *)code + size);
	double t0 = now();
	mprotect(code, size, PROT_READ | PROT_EXEC);
	double t1 = now();
	long sum = 0;
	for (int i = 0; i < n; i++)
		sum += ((long (*)(void))(code + 4 * i))();
	double t2 = now();
	for (int i = 0; i < n; i++)
		sum += ((long (*)(void))(code + 4 * i))();
	double t3 = now();
	long expect = 2L * n * getppid();
	printf("mprotect_us %.1f first_ns %.1f second_ns %.1f ok %d\n", (t1 - t0) / 1e3,
	       (t2 - t1) / n, (t3 - t2) / n, sum == expect);
	return 0;
}

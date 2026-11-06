// Target for the bpf_probe_read cost (more-dims-20261106/n1-probe-read): one thread on CPU 5
// calls probe_me(s) for SECS seconds, s a string of LEN bytes (plus its NUL) that starts 128
// bytes into a page. Prints total_ops and ops_per_sec. PVR_CHECK=1 first tries
// process_vm_readv on the process itself (what bpftime's checked probe reads do) and prints
// "pvr_self <result> errno <errno>".
#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

// Long enough for Gum's 16-byte redirect: in a container Gum refused the two-instruction
// version (GUM_ATTACH_WRONG_SIGNATURE), which it hooks on the host.
__attribute__((noinline)) unsigned long probe_me(const char *s)
{
	unsigned long h = 0;
	for (int i = 0; i < 4; i++)
		h = h * 31 + (unsigned char)s[i];
	__asm__ volatile("" : : "r"(h) : "memory");
	return h;
}

// BAD=1/2/3: call probe_bad(p) instead, p NULL, an unmapped page or a kernel address; the
// function never touches p, only the probe reads it (a tool reading a bad user pointer).
__attribute__((noinline)) unsigned long probe_bad(const char *p)
{
	unsigned long h = (unsigned long)p;
	for (int i = 0; i < 4; i++)
		h = h * 31 + i;
	__asm__ volatile("" : : "r"(h) : "memory");
	return h;
}

static double now(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: readstr LEN SECS\n");
		return 1;
	}
	int len = atoi(argv[1]), secs = atoi(argv[2]);
	if (len < 0 || len > 8000 || secs < 1)
		return 1;
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(5, &set);
	sched_setaffinity(0, sizeof(set), &set);
	char *page = aligned_alloc(4096, 3 * 4096);
	char *s = page + 128;
	memset(s, 'a', len);
	s[len] = 0;
	if (getenv("PVR_CHECK")) {
		char tmp[16];
		struct iovec l = { tmp, sizeof(tmp) }, r = { s, sizeof(tmp) };
		errno = 0;
		long n = process_vm_readv(getpid(), &l, 1, &r, 1, 0);
		printf("pvr_self %ld errno %d\n", n, n < 0 ? errno : 0);
	}
	unsigned long calls = 0, acc = 0;
	double end = now() + secs;
	const char *bad = NULL;
	int badkind = getenv("BAD") ? atoi(getenv("BAD")) : 0;
	if (badkind == 2) {
		char *m = mmap(NULL, 4096, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		munmap(m, 4096);
		bad = m;
	} else if (badkind == 3) {
		bad = (const char *)0xffff800008000000UL;
	}
	if (badkind) {
		printf("bad_pointer %p\n", (void *)bad);
		fflush(stdout);
		do {
			for (int i = 0; i < 256; i++)
				acc += probe_bad(bad);
			calls += 256;
		} while (now() < end);
	} else {
		do {
			for (int i = 0; i < 256; i++)
				acc += probe_me(s);
			calls += 256;
		} while (now() < end);
	}
	printf("total_ops %lu\nops_per_sec %.1f\nacc %lu\n", calls, calls / (double)secs, acc & 1);
	return 0;
}

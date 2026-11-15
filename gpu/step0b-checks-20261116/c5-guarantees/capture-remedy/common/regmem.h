// regmem.h: helpers shared by the check-3 programs (step 0, e3-copyout).
// Host memory "like bpftime's segment": POSIX shm + mmap(MAP_SHARED) + read
// prefault + cuMemHostRegister(DEVICEMAP | PORTABLE), as in
// runtime/src/bpftime_shm_internal.cpp:1414-1469 of dev 52126f33.
// The shm name is unlinked right after mmap, so nothing is left in /dev/shm
// even if the process is killed.
#pragma once
#include <cuda.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static inline const char *cu_name(CUresult r)
{
	const char *s = NULL;
	if (cuGetErrorName(r, &s) != CUDA_SUCCESS || !s)
		return "?";
	return s;
}

#define CK(x)                                                                  \
	do {                                                                   \
		CUresult r_ = (x);                                             \
		if (r_ != CUDA_SUCCESS) {                                      \
			fprintf(stderr, "%s:%d %s -> %d %s\n", __FILE__,       \
				__LINE__, #x, (int)r_, cu_name(r_));           \
			exit(2);                                               \
		}                                                              \
	} while (0)

static inline double now_s(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static inline uint64_t now_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

typedef struct {
	void *p;
	size_t n;
	CUresult reg; // result of cuMemHostRegister
} regbuf_t;

// Returns 0 on success. tag must be short ([a-z0-9]).
static inline int regbuf_create(regbuf_t *b, size_t n, const char *tag)
{
	const size_t pg = 4096;
	char name[96];
	n = (n + pg - 1) / pg * pg;
	memset(b, 0, sizeof(*b));
	snprintf(name, sizeof(name), "/step0b_c5_%s_%d", tag, (int)getpid());
	int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
	if (fd < 0) {
		fprintf(stderr, "shm_open %s: %s\n", name, strerror(errno));
		return -1;
	}
	if (ftruncate(fd, (off_t)n) != 0) {
		fprintf(stderr, "ftruncate: %s\n", strerror(errno));
		close(fd);
		shm_unlink(name);
		return -1;
	}
	void *p = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	int e = errno;
	close(fd);
	shm_unlink(name);
	if (p == MAP_FAILED) {
		fprintf(stderr, "mmap: %s\n", strerror(e));
		return -1;
	}
	// prefault as bpftime does (read every page)
	volatile unsigned char sink = 0;
	for (size_t i = 0; i < n; i += pg)
		sink ^= ((volatile unsigned char *)p)[i];
	(void)sink;
	b->p = p;
	b->n = n;
	b->reg = cuMemHostRegister(p, n,
				   CU_MEMHOSTREGISTER_DEVICEMAP |
					   CU_MEMHOSTREGISTER_PORTABLE);
	if (b->reg != CUDA_SUCCESS) {
		fprintf(stderr, "cuMemHostRegister: %d %s\n", (int)b->reg,
			cu_name(b->reg));
		munmap(p, n);
		b->p = NULL;
		return -1;
	}
	return 0;
}

static inline void regbuf_destroy(regbuf_t *b)
{
	if (!b->p)
		return;
	cuMemHostUnregister(b->p);
	munmap(b->p, b->n);
	b->p = NULL;
}

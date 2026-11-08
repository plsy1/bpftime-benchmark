// shmtool.c - second-process helper (no CUDA): maps a /dev/shm file MAP_SHARED and
// checks / fills / handshakes on it.
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static double now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

static void *map_file(const char *path, size_t *size_out)
{
	int fd = open(path, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(2);
	}
	struct stat st;
	fstat(fd, &st);
	void *p = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (p == MAP_FAILED) {
		perror("mmap");
		exit(2);
	}
	*size_out = st.st_size;
	return p;
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: shmtool check|fill|hs <path> ...\n");
		return 2;
	}
	size_t size;
	unsigned *p = (unsigned *)map_file(argv[2], &size);
	printf("[shmtool pid=%d] mapped %s size=%zu at %p\n", getpid(), argv[2], size, (void *)p);
	if (!strcmp(argv[1], "check") && argc >= 6) {
		// check <path> <stride_ints> <n> <add> : expect p[i*stride] == i + add
		size_t stride = strtoull(argv[3], 0, 0), n = strtoull(argv[4], 0, 0);
		unsigned add = strtoul(argv[5], 0, 0);
		size_t bad = 0, first = (size_t)-1;
		for (size_t i = 0; i < n; i++) {
			unsigned v = __atomic_load_n(&p[i * stride], __ATOMIC_ACQUIRE);
			if (v != (unsigned)(i + add)) {
				if (first == (size_t)-1)
					first = i;
				bad++;
			}
		}
		printf("[shmtool] check n=%zu stride=%zu add=0x%x mismatches=%zu", n, stride, add, bad);
		if (bad)
			printf(" first_bad_i=%zu value=0x%x expected=0x%x", first, p[first * stride], (unsigned)(first + add));
		printf("\n");
		return bad ? 1 : 0;
	}
	if (!strcmp(argv[1], "fill") && argc >= 6) {
		// fill <path> <stride_ints> <n> <base> : p[i*stride] = base + i
		size_t stride = strtoull(argv[3], 0, 0), n = strtoull(argv[4], 0, 0);
		unsigned base = strtoul(argv[5], 0, 0);
		for (size_t i = 0; i < n; i++)
			__atomic_store_n(&p[i * stride], base + (unsigned)i, __ATOMIC_RELEASE);
		printf("[shmtool] filled n=%zu stride=%zu base=0x%x\n", n, stride, base);
		return 0;
	}
	if (!strcmp(argv[1], "hs") && argc >= 6) {
		// hs <path> <flag1_off_bytes> <flag2_off_bytes> <timeout_ms>:
		// wait for flag1==1, then set flag2=1 (mimics the host side of bpftime's helper-call handshake)
		size_t o1 = strtoull(argv[3], 0, 0), o2 = strtoull(argv[4], 0, 0);
		double to = atof(argv[5]);
		volatile unsigned *f1 = (volatile unsigned *)((char *)p + o1);
		volatile unsigned *f2 = (volatile unsigned *)((char *)p + o2);
		double t0 = now_ms();
		unsigned long polls = 0;
		while (__atomic_load_n(f1, __ATOMIC_ACQUIRE) != 1) {
			polls++;
			if (now_ms() - t0 > to) {
				printf("[shmtool] hs TIMEOUT waiting flag1 after %.1f ms (%lu polls) flag1=%u\n", now_ms() - t0,
				       polls, *f1);
				return 1;
			}
		}
		double t1 = now_ms();
		__atomic_store_n(f1, 0, __ATOMIC_RELEASE);
		__atomic_store_n(f2, 1, __ATOMIC_RELEASE);
		printf("[shmtool] hs saw flag1 after %.3f ms (%lu polls); set flag2=1\n", t1 - t0, polls);
		return 0;
	}
	fprintf(stderr, "bad args\n");
	return 2;
}

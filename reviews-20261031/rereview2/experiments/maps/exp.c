// Experiments against the preloaded bpftime syscall server through raw bpf().
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <linux/bpf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

static long bpf(int cmd, union bpf_attr *attr)
{
	return syscall(__NR_bpf, cmd, attr, sizeof(*attr));
}

static int count_fds(void)
{
	int n = 0;
	DIR *d = opendir("/proc/self/fd");
	struct dirent *e;
	while ((e = readdir(d)) != NULL)
		n++;
	closedir(d);
	return n;
}

static long create(int type, uint32_t key, uint32_t value, uint32_t max,
		   const char *name)
{
	union bpf_attr a;
	memset(&a, 0, sizeof(a));
	a.map_type = type;
	a.key_size = key;
	a.value_size = value;
	a.max_entries = max;
	strncpy(a.map_name, name, sizeof(a.map_name) - 1);
	errno = 0;
	long r = bpf(BPF_MAP_CREATE, &a);
	int e = errno;
	printf("create %-12s type=%d key=%u value=%u max=%u -> %ld errno=%d (%s)\n",
	       name, type, key, value, max, r, e, strerror(e));
	errno = e;
	return r;
}

static long lookup(int fd, const void *key, void *value)
{
	union bpf_attr a;
	memset(&a, 0, sizeof(a));
	a.map_fd = fd;
	a.key = (uint64_t)(uintptr_t)key;
	a.value = (uint64_t)(uintptr_t)value;
	errno = 0;
	return bpf(BPF_MAP_LOOKUP_ELEM, &a);
}

static long update(int fd, const void *key, const void *value, uint64_t flags)
{
	union bpf_attr a;
	memset(&a, 0, sizeof(a));
	a.map_fd = fd;
	a.key = (uint64_t)(uintptr_t)key;
	a.value = (uint64_t)(uintptr_t)value;
	a.flags = flags;
	errno = 0;
	return bpf(BPF_MAP_UPDATE_ELEM, &a);
}

static long lookup_and_delete(int fd, const void *key, void *value)
{
	union bpf_attr a;
	memset(&a, 0, sizeof(a));
	a.map_fd = fd;
	a.key = (uint64_t)(uintptr_t)key;
	a.value = (uint64_t)(uintptr_t)value;
	errno = 0;
	return bpf(BPF_MAP_LOOKUP_AND_DELETE_ELEM, &a);
}

static int ncpus(void)
{
	// libbpf's count: bits in /sys/devices/system/cpu/possible
	FILE *f = fopen("/sys/devices/system/cpu/possible", "r");
	char line[256];
	if (!f || !fgets(line, sizeof(line), f))
		return 1;
	fclose(f);
	int n = 0;
	char *p = line;
	while (*p && *p != '\n') {
		char *end;
		long a = strtol(p, &end, 10), b = a;
		p = end;
		if (*p == '-') {
			b = strtol(p + 1, &end, 10);
			p = end;
		}
		n += b - a + 1;
		if (*p == ',')
			p++;
	}
	return n;
}

static void layout(int type, uint32_t value_size)
{
	const int n = ncpus();
	const size_t stride = (value_size + 7) & ~7u;
	const size_t size = stride * n;
	long fd = create(type, 4, value_size, 4, "layout");
	if (fd < 0)
		return;
	uint8_t *in = calloc(1, size + 64);
	uint8_t *out = calloc(1, size + 64);
	for (int c = 0; c < n; c++)
		for (uint32_t i = 0; i < value_size; i++)
			in[c * stride + i] = (uint8_t)(16 * c + i + 1);
	memset(out, 0xee, size + 64);
	uint32_t key = 1;
	long r = update(fd, &key, in, 0);
	printf("  update -> %ld errno=%d\n", r, errno);
	r = lookup(fd, &key, out);
	printf("  lookup -> %ld errno=%d\n", r, errno);
	int bad = 0;
	for (size_t i = 0; i < size; i++) {
		size_t c = i / stride, off = i % stride;
		uint8_t want = off < value_size ? (uint8_t)(16 * c + off + 1) : 0;
		if (out[i] != want) {
			if (bad < 5)
				printf("  byte %zu: got %02x want %02x\n", i,
				       out[i], want);
			bad++;
		}
	}
	int guard_bad = 0;
	for (size_t i = size; i < size + 64; i++)
		if (out[i] != 0xee)
			guard_bad++;
	printf("  lookup: %d wrong bytes, %d guard bytes touched\n", bad,
	       guard_bad);
	memset(out, 0xee, size + 64);
	r = lookup_and_delete(fd, &key, out);
	printf("  lookup_and_delete -> %ld errno=%d\n", r, errno);
	if (r == 0) {
		bad = 0;
		for (size_t i = 0; i < size; i++) {
			size_t c = i / stride, off = i % stride;
			uint8_t want = off < value_size ?
					       (uint8_t)(16 * c + off + 1) :
					       0;
			if (out[i] != want)
				bad++;
		}
		guard_bad = 0;
		for (size_t i = size; i < size + 64; i++)
			if (out[i] != 0xee)
				guard_bad++;
		printf("  lookup_and_delete: %d wrong bytes, %d guard bytes touched\n",
		       bad, guard_bad);
		r = lookup(fd, &key, out);
		printf("  lookup after -> %ld errno=%d (%s)\n", r, errno,
		       strerror(errno));
	}
	free(in);
	free(out);
}

int main(int argc, char **argv)
{
	const char *mode = argc > 1 ? argv[1] : "";
	if (strcmp(mode, "unsupported") == 0) {
		create(BPF_MAP_TYPE_SOCKHASH, 4, 4, 4, "sockhash");
		create(BPF_MAP_TYPE_LRU_PERCPU_HASH, 4, 4, 4, "lrupcpu");
		create(BPF_MAP_TYPE_BLOOM_FILTER, 4, 4, 4, "bloomkey");
	} else if (strcmp(mode, "percpu_key") == 0) {
		// Kernel accepts; bpftime's bound multiplies the key by ncpu.
		create(BPF_MAP_TYPE_PERCPU_HASH, 256, 8, 100000, "pcpu256");
		create(BPF_MAP_TYPE_PERCPU_HASH, 64, 8, 400000, "pcpu64");
		create(BPF_MAP_TYPE_HASH, 256, 8, 100000, "hash256");
	} else if (strcmp(mode, "fdleak") == 0) {
		int before = count_fds();
		for (int i = 0; i < 200; i++) {
			union bpf_attr a;
			memset(&a, 0, sizeof(a));
			a.map_type = BPF_MAP_TYPE_HASH;
			a.key_size = 0;
			a.value_size = 8;
			a.max_entries = 4;
			bpf(BPF_MAP_CREATE, &a);
		}
		int after = count_fds();
		printf("fds before %d after %d (200 rejected creations)\n",
		       before, after);
	} else if (strcmp(mode, "lenerr") == 0) {
		// Segment of BPFTIME_SHM_MEMORY_MB=16: lower bound 16*600000 =
		// 9.6 MB passes, the striped map wants next_prime(1.2M) * 24
		// = 28.8 MB.
		int before = count_fds();
		long r = create(BPF_MAP_TYPE_HASH, 8, 8, 600000, "biglen");
		int after = count_fds();
		printf("fds before %d after %d\n", before, after);
		// Sanity: a map that fits still works.
		long ok = create(BPF_MAP_TYPE_HASH, 8, 8, 1000, "small");
		printf("small -> %ld\n", ok);
		if (r < 0 && argc > 2) {
			// The leaked fake fd is the one below the sane map's.
			int stale = atoi(argv[2]) ? atoi(argv[2]) : (int)ok - 1;
			uint64_t key = 1, value = 0;
			printf("lookup on stale fd %d ...\n", stale);
			fflush(stdout);
			long lr = lookup(stale, &key, &value);
			printf("lookup stale -> %ld errno=%d (%s)\n", lr, errno,
			       strerror(errno));
		}
	} else if (strcmp(mode, "types") == 0) {
		int which = argc > 2 ? atoi(argv[2]) : 0;
		switch (which) {
		case 0: create(BPF_MAP_TYPE_LPM_TRIE, 0, 4, 4, "lpm0"); break;
		case 1: {
			long fd = create(BPF_MAP_TYPE_LPM_TRIE, 4, 4, 4, "lpm4");
			if (fd >= 0) {
				uint32_t k = 0x08000001, v = 1;
				long r = update(fd, &k, &v, 0);
				printf("  lpm update -> %ld errno=%d\n", r, errno);
			}
			break;
		}
		case 2: create(BPF_MAP_TYPE_STACK_TRACE, 4, 0, 4, "st0"); break;
		case 3: {
			long fd = create(BPF_MAP_TYPE_STACK_TRACE, 4, 12, 4, "st12");
			if (fd >= 0) {
				uint32_t k = 0; uint8_t v[16] = {0};
				long r = lookup(fd, &k, v);
				printf("  st lookup -> %ld errno=%d\n", r, errno);
			}
			break;
		}
		case 4: create(BPF_MAP_TYPE_BLOOM_FILTER, 0, 0, 4, "bloom0"); break;
		case 5: create(BPF_MAP_TYPE_RINGBUF, 0, 0, 2048, "rb2048"); break;
		case 6: create(BPF_MAP_TYPE_PERF_EVENT_ARRAY, 4, 4, 0, "pea0"); break;
		case 7: create(BPF_MAP_TYPE_ARRAY_OF_MAPS, 4, 4, 0, "aom0"); break;
		case 8: create(BPF_MAP_TYPE_PROG_ARRAY, 4, 4, 0, "pa0"); break;
		case 9: create(BPF_MAP_TYPE_RINGBUF, 0, 0, 1, "rb1"); break;
		case 10: {
			long fd = create(BPF_MAP_TYPE_QUEUE, 0, 8, 1u << 30, "qbig");
			printf("  queue -> %ld\n", fd);
			break;
		}
		case 11: create(BPF_MAP_TYPE_PERCPU_ARRAY, 4, 40000, 1, "pcpu40k"); break;
		case 12: create(BPF_MAP_TYPE_ARRAY, 4, 8, 0xffffffffu, "arrmax"); break;
		}
	} else if (strcmp(mode, "layout") == 0) {
		int type = argc > 2 ? atoi(argv[2]) : BPF_MAP_TYPE_PERCPU_ARRAY;
		uint32_t vs = argc > 3 ? (uint32_t)atoi(argv[3]) : 4;
		layout(type, vs);
	} else if (strcmp(mode, "lad") == 0) {
		long fd = create(BPF_MAP_TYPE_ARRAY, 4, 8, 4, "arr");
		uint32_t k = 1; uint64_t v = 0;
		long r = lookup_and_delete(fd, &k, &v);
		printf("array lookup_and_delete -> %ld errno=%d\n", r, errno);
		fd = create(BPF_MAP_TYPE_LRU_HASH, 4, 8, 4, "lru");
		v = 77;
		r = update(fd, &k, &v, 0);
		printf("lru update -> %ld errno=%d\n", r, errno);
		v = 0;
		r = lookup_and_delete(fd, &k, &v);
		printf("lru lookup_and_delete -> %ld errno=%d value=%lu\n", r, errno, (unsigned long)v);
		r = lookup_and_delete(fd, &k, &v);
		printf("lru lookup_and_delete again -> %ld errno=%d\n", r, errno);
		fd = create(BPF_MAP_TYPE_HASH, 4, 8, 4, "hash");
		v = 99;
		update(fd, &k, &v, 0);
		v = 0;
		r = lookup_and_delete(fd, &k, &v);
		printf("hash lookup_and_delete -> %ld errno=%d value=%lu\n", r, errno, (unsigned long)v);
		r = lookup_and_delete(fd, &k, &v);
		printf("hash lookup_and_delete again -> %ld errno=%d\n", r, errno);
		// flags: BPF_F_LOCK on a hash without spin lock: kernel EINVAL
		union bpf_attr a; memset(&a, 0, sizeof(a));
		a.map_fd = fd; a.key = (uint64_t)(uintptr_t)&k; a.value = (uint64_t)(uintptr_t)&v; a.flags = 8;
		update(fd, &k, &v, 0);
		errno = 0;
		r = bpf(BPF_MAP_LOOKUP_AND_DELETE_ELEM, &a);
		printf("hash lookup_and_delete flags=8 -> %ld errno=%d\n", r, errno);
	}
	return 0;
}

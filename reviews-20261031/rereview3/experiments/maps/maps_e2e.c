// Creates maps the way libbpf-based tools do, through bpf_map_create, and
// exercises lookup-and-delete; run under LD_PRELOAD of the syscall server.
#define _GNU_SOURCE
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fails;

static int nfds(void)
{
	int n = 0;
	DIR *d = opendir("/proc/self/fd");
	while (d && readdir(d))
		n++;
	if (d)
		closedir(d);
	return n;
}

static int mk(const char *what, enum bpf_map_type t, uint32_t ks, uint32_t vs,
	      uint32_t me, uint32_t flags, int inner)
{
	LIBBPF_OPTS(bpf_map_create_opts, o, .map_flags = flags,
		    .inner_map_fd = inner > 0 ? inner : 0);
	errno = 0;
	int fd = bpf_map_create(t, what, ks, vs, me, &o);
	printf("%-28s type=%2d k=%u v=%u n=%u -> fd=%d errno=%d\n", what, t,
	       ks, vs, me, fd, fd < 0 ? -fd : errno);
	if (fd < 0)
		fails++;
	return fd;
}

static void expect_fail(const char *what, enum bpf_map_type t, uint32_t ks,
			uint32_t vs, uint32_t me, int want)
{
	errno = 0;
	int fd = bpf_map_create(t, what, ks, vs, me, NULL);
	int e = fd < 0 ? -fd : 0;
	printf("%-28s expect -%d: fd=%d err=%d %s\n", what, want, fd, e,
	       fd < 0 && e == want ? "ok" : "MISMATCH");
	if (fd >= 0) {
		fails++;
		close(fd);
	}
}

int main(void)
{
	int ncpu = libbpf_num_possible_cpus();
	printf("possible cpus: %d\n", ncpu);
	int before = nfds();

	// What real tools create.
	int hash = mk("hash_8_8", BPF_MAP_TYPE_HASH, 8, 8, 10240, 0, 0);
	int hash_big = mk("hash_memleak_allocs", BPF_MAP_TYPE_HASH, 8, 16,
			  1000000, 0, 0);
	int lru = mk("lru_hash", BPF_MAP_TYPE_LRU_HASH, 4, 8, 8192, 0, 0);
	int pch = mk("percpu_hash_4_8", BPF_MAP_TYPE_PERCPU_HASH, 4, 8, 4096,
		     0, 0);
	int pch256 = mk("percpu_hash_256key", BPF_MAP_TYPE_PERCPU_HASH, 256,
			8, 100000, 0, 0);
	int bt = mk("bpftrace_percpu_hash", BPF_MAP_TYPE_PERCPU_HASH, 64, 8,
		    4096, 0, 0);
	int arr = mk("array_rodata", BPF_MAP_TYPE_ARRAY, 4, 4096, 1,
		     BPF_F_RDONLY_PROG, 0);
	int bss = mk("array_bss", BPF_MAP_TYPE_ARRAY, 4, 16, 1, 0, 0);
	int arr_big = mk("array_32m_value", BPF_MAP_TYPE_ARRAY, 4, 32 << 20, 1,
			 0, 0);
	int pca = mk("percpu_array", BPF_MAP_TYPE_PERCPU_ARRAY, 4, 8, 1024, 0,
		     0);
	int pca_hist = mk("percpu_array_hist", BPF_MAP_TYPE_PERCPU_ARRAY, 4,
			  4096, 64, 0, 0);
	int q = mk("queue", BPF_MAP_TYPE_QUEUE, 0, 8, 64, 0, 0);
	int st = mk("stack", BPF_MAP_TYPE_STACK, 0, 8, 64, 0, 0);
	int rb = mk("ringbuf", BPF_MAP_TYPE_RINGBUF, 0, 0, 256 << 10, 0, 0);
	int pea = mk("perf_event_array", BPF_MAP_TYPE_PERF_EVENT_ARRAY, 4, 4,
		     ncpu, 0, 0);
	int stt = mk("stack_trace", BPF_MAP_TYPE_STACK_TRACE, 4, 127 * 8,
		     10240, 0, 0);
	int lpm = mk("lpm_trie", BPF_MAP_TYPE_LPM_TRIE, 8, 4, 1024,
		     BPF_F_NO_PREALLOC, 0);
	int bloom = mk("bloom", BPF_MAP_TYPE_BLOOM_FILTER, 0, 4, 1024, 0, 0);
	int cg = mk("cgroup_array", BPF_MAP_TYPE_CGROUP_ARRAY, 4, 4, 1, 0, 0);
	int inner = mk("inner_hash", BPF_MAP_TYPE_HASH, 4, 4, 16, 0, 0);
	int aom = mk("array_of_maps", BPF_MAP_TYPE_ARRAY_OF_MAPS, 4, 4, 4, 0,
		     inner);
	int pa = mk("prog_array", BPF_MAP_TYPE_PROG_ARRAY, 4, 4, 8, 0, 0);
	(void)hash_big; (void)pch256; (void)bt; (void)arr; (void)bss;
	(void)arr_big; (void)pca; (void)pca_hist; (void)rb; (void)pea;
	(void)stt; (void)lpm; (void)bloom; (void)cg; (void)aom; (void)pa;

	// lookup-and-delete as libbpf-tools call it.
	if (hash >= 0) {
		uint64_t k = 42, v = 7, out = 0;
		if (bpf_map_update_elem(hash, &k, &v, BPF_ANY))
			printf("hash update failed %d\n", errno), fails++;
		int r = bpf_map_lookup_and_delete_elem(hash, &k, &out);
		printf("hash lad: r=%d out=%lu (want 7)\n", r,
		       (unsigned long)out);
		if (r || out != 7)
			fails++;
		errno = 0;
		r = bpf_map_lookup_elem(hash, &k, &out);
		printf("hash lookup after lad: r=%d errno=%d (want ENOENT)\n",
		       r, -r);
		if (r != -ENOENT)
			fails++;
	}
	if (lru >= 0) {
		uint32_t k = 5;
		uint64_t v = 99, out = 0;
		bpf_map_update_elem(lru, &k, &v, BPF_ANY);
		int r = bpf_map_lookup_and_delete_elem(lru, &k, &out);
		printf("lru lad: r=%d out=%lu (want 99)\n", r,
		       (unsigned long)out);
		if (r || out != 99)
			fails++;
	}
	if (pch >= 0) {
		uint32_t k = 9;
		uint64_t *vals = calloc(ncpu, 8), *out = calloc(ncpu, 8);
		for (int i = 0; i < ncpu; i++)
			vals[i] = 100 + i;
		if (bpf_map_update_elem(pch, &k, vals, BPF_ANY))
			printf("pch update failed %d\n", errno), fails++;
		int r = bpf_map_lookup_and_delete_elem(pch, &k, out);
		int ok = r == 0;
		for (int i = 0; i < ncpu; i++)
			if (out[i] != 100 + (uint64_t)i)
				ok = 0;
		printf("percpu hash lad: r=%d values %s\n", r,
		       ok ? "ok" : "WRONG");
		if (!ok)
			fails++;
		free(vals);
		free(out);
	}
	if (q >= 0) {
		uint64_t v1 = 11, v2 = 22, out = 0;
		bpf_map_update_elem(q, NULL, &v1, 0);
		bpf_map_update_elem(q, NULL, &v2, 0);
		int r = bpf_map_lookup_and_delete_elem(q, NULL, &out);
		printf("queue lad (NULL key): r=%d out=%lu (want 11)\n", r,
		       (unsigned long)out);
		if (r || out != 11)
			fails++;
		r = bpf_map_lookup_and_delete_elem(q, NULL, &out);
		r = bpf_map_lookup_and_delete_elem(q, NULL, &out);
		printf("queue lad empty: r=%d (want -ENOENT=%d)\n", r, -ENOENT);
		if (r != -ENOENT)
			fails++;
	}
	if (st >= 0) {
		uint64_t v1 = 11, v2 = 22, out = 0;
		bpf_map_update_elem(st, NULL, &v1, 0);
		bpf_map_update_elem(st, NULL, &v2, 0);
		int r = bpf_map_lookup_and_delete_elem(st, NULL, &out);
		printf("stack lad (NULL key): r=%d out=%lu (want 22)\n", r,
		       (unsigned long)out);
		if (r || out != 22)
			fails++;
		// flags variant with 0 flags, as libbpf's bpf_map__ wrapper
		r = bpf_map_lookup_and_delete_elem_flags(st, NULL, &out, 0);
		printf("stack lad flags=0: r=%d out=%lu (want 11)\n", r,
		       (unsigned long)out);
		if (r || out != 11)
			fails++;
	}

	int mid = nfds();
	// Rejections: the kernel refuses these too.
	for (int i = 0; i < 20; i++)
		expect_fail("hash_key0", BPF_MAP_TYPE_HASH, 0, 8, 4, EINVAL);
	expect_fail("array_key8", BPF_MAP_TYPE_ARRAY, 8, 8, 4, EINVAL);
	expect_fail("queue_key4", BPF_MAP_TYPE_QUEUE, 4, 8, 4, EINVAL);
	expect_fail("sockhash", BPF_MAP_TYPE_SOCKHASH, 4, 4, 4, EINVAL);
	expect_fail("lru_percpu_hash", BPF_MAP_TYPE_LRU_PERCPU_HASH, 4, 4, 4,
		    EINVAL);
	// Fits the lower bound, not the segment: ENOMEM, no crash.
	for (int i = 0; i < 3; i++)
		expect_fail("hash_oom", BPF_MAP_TYPE_HASH, 8, 8, 6000000,
			    ENOMEM);
	int after = nfds();
	printf("fds: before=%d after-creates=%d after-rejections=%d %s\n",
	       before, mid, after, mid == after ? "ok" : "LEAK");
	if (mid != after)
		fails++;
	// The table still works after the failures.
	if (hash >= 0) {
		uint64_t k = 1, v = 2, out = 0;
		bpf_map_update_elem(hash, &k, &v, BPF_ANY);
		bpf_map_lookup_elem(hash, &k, &out);
		printf("hash still usable: out=%lu (want 2)\n",
		       (unsigned long)out);
		if (out != 2)
			fails++;
	}
	printf("FAILS=%d\n", fails);
	return fails ? 1 : 0;
}

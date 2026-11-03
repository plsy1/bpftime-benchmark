// Loader: picks two keys of one bucket of bpf's htab with seed 0 (jhash2 of
// the 4-byte key, & (n_buckets - 1)), checks that with next_key order, then
// runs 2 updater threads and 3 reader threads for SECS seconds, each calling
// BPF_PROG_TEST_RUN with a repeat count in a loop, and prints the counts.
// Usage: r1 SECS ROUNDS [CPUS]: CPUS = u0,u1,r0,r1,r2 (updaters, readers).
#define _GNU_SOURCE
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static inline uint32_t rol32(uint32_t w, unsigned s) { return (w << s) | (w >> (32 - s)); }
// include/linux/jhash.h: jhash2(k, 1, initval)
static uint32_t jhash2_1(uint32_t k, uint32_t initval)
{
	uint32_t a, b, c;
	a = b = c = 0xdeadbeef + (1 << 2) + initval;
	a += k;
	c ^= b; c -= rol32(b, 14);
	a ^= c; a -= rol32(c, 11);
	b ^= a; b -= rol32(a, 25);
	c ^= b; c -= rol32(b, 16);
	a ^= c; a -= rol32(c, 4);
	b ^= a; b -= rol32(a, 14);
	c ^= b; c -= rol32(b, 24);
	return c;
}
#define N_BUCKETS 64 /* roundup_pow_of_two(max_entries) */
static uint32_t bucket(uint32_t k) { return jhash2_1(k, 0) & (N_BUCKETS - 1); }

struct value { uint64_t key, a, b; };
struct stats { uint64_t reads, misses, torn, updates; };

static atomic_int stop;
static double now(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec / 1e9;
}
static int prog_fd[3];
static int cpus[5] = { -1, -1, -1, -1, -1 };
static void *runner(void *arg)
{
	const int i = (int)(intptr_t)arg;
	if (cpus[i] >= 0) {
		cpu_set_t set;
		CPU_ZERO(&set);
		CPU_SET(cpus[i], &set);
		if (pthread_setaffinity_np(pthread_self(), sizeof(set), &set)) {
			perror("affinity");
			exit(1);
		}
	}
	int fd = prog_fd[i < 2 ? i : 2];
	char pkt[64] = { 0 };
	LIBBPF_OPTS(bpf_test_run_opts, o, .data_in = pkt, .data_size_in = sizeof(pkt),
		    .repeat = 1000);
	while (!atomic_load(&stop))
		if (bpf_prog_test_run_opts(fd, &o)) {
			perror("test_run");
			exit(1);
		}
	return NULL;
}

int main(int argc, char **argv)
{
	const int secs = argc > 1 ? atoi(argv[1]) : 2;
	const int rounds = argc > 2 ? atoi(argv[2]) : 5;
	if (argc > 3 && sscanf(argv[3], "%d,%d,%d,%d,%d", &cpus[0], &cpus[1], &cpus[2],
				&cpus[3], &cpus[4]) != 5) {
		fprintf(stderr, "CPUS: u0,u1,r0,r1,r2\n");
		return 2;
	}
	uint32_t k0 = 1, k1 = 0;
	for (uint32_t k = 2; !k1; k++)
		if (bucket(k) == bucket(k0))
			k1 = k;
	printf("keys %u and %u, bucket %u of %u (jhash2, seed 0)\n", k0, k1, bucket(k0), N_BUCKETS);
	int ncpu = libbpf_num_possible_cpus();
	for (int round = 0; round < rounds; round++) {
		struct bpf_object *obj = bpf_object__open_file("r1.bpf.o", NULL);
		if (!obj) { perror("open"); return 1; }
		struct bpf_map *rodata = bpf_object__find_map_by_name(obj, "r1.rodata");
		if (!rodata) { fprintf(stderr, "no rodata map\n"); return 1; }
		uint32_t keys[2] = { k0, k1 };
		if (bpf_map__set_initial_value(rodata, keys, sizeof(keys))) { fprintf(stderr, "rodata\n"); return 1; }
		if (bpf_object__load(obj)) { perror("load"); return 1; }
		int table = bpf_object__find_map_fd_by_name(obj, "table");
		int counts = bpf_object__find_map_fd_by_name(obj, "counts");
		prog_fd[0] = bpf_program__fd(bpf_object__find_program_by_name(obj, "update0"));
		prog_fd[1] = bpf_program__fd(bpf_object__find_program_by_name(obj, "update1"));
		prog_fd[2] = bpf_program__fd(bpf_object__find_program_by_name(obj, "reader"));
		if (round == 0) {
			// Bucket check: 24 keys with seed 0 come back from next_key in
			// bucket order (and every key once).
			uint32_t probe[24];
			struct value pv = { 0 };
			for (int i = 0; i < 24; i++) {
				probe[i] = 1000 + i * 7919;
				bpf_map_update_elem(table, &probe[i], &pv, BPF_ANY);
			}
			uint32_t cur, nxt, last_b = 0, n = 0;
			int ok = 1;
			void *at = NULL;
			while (!bpf_map_get_next_key(table, at, &nxt)) {
				if (bucket(nxt) < last_b) ok = 0;
				last_b = bucket(nxt);
				cur = nxt; at = &cur; n++;
			}
			printf("bucket check: %u keys, next_key in computed bucket order: %s\n", n, ok ? "yes" : "NO");
			for (int i = 0; i < 24; i++)
				bpf_map_delete_elem(table, &probe[i]);
			if (!ok || n != 24) return 1;
		}
		// k0 first, so k1 is the head and k0 is behind it.
		struct value v0 = { k0, 0, 0 }, v1 = { k1, 0, 0 };
		bpf_map_update_elem(table, &k0, &v0, BPF_NOEXIST);
		bpf_map_update_elem(table, &k1, &v1, BPF_NOEXIST);
		pthread_t t[5];
		atomic_store(&stop, 0);
		for (intptr_t i = 0; i < 5; i++)
			pthread_create(&t[i], NULL, runner, (void *)i);
		const double t0 = now();
		usleep(secs * 1000000);
		atomic_store(&stop, 1);
		for (int i = 0; i < 5; i++) pthread_join(t[i], NULL);
		const double dt = now() - t0;
		struct stats per[ncpu], sum = { 0 };
		uint32_t zero = 0;
		bpf_map_lookup_elem(counts, &zero, per);
		for (int c = 0; c < ncpu; c++) {
			sum.reads += per[c].reads; sum.misses += per[c].misses;
			sum.torn += per[c].torn; sum.updates += per[c].updates;
		}
		printf("round %d: %.2f s, reads %llu (%.1f M/s), updates %llu (%.1f M/s), misses %llu (%.4f%%), torn %llu\n",
		       round + 1, dt, (unsigned long long)sum.reads, sum.reads / dt / 1e6,
		       (unsigned long long)sum.updates, sum.updates / dt / 1e6,
		       (unsigned long long)sum.misses, sum.reads ? 100.0 * sum.misses / sum.reads : 0.0,
		       (unsigned long long)sum.torn);
		fflush(stdout);
		bpf_object__close(obj);
	}
	return 0;
}

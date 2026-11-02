// Kernel BPF hash (preallocated, 100k entries, 8 B key and value) through bpf():
// fill to a load, then delete a random live key and insert a new one. Includes the
// two system calls per operation; shows whether the cost depends on the load.
#include <bpf/bpf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static uint64_t key(uint64_t id) { uint64_t x = id * 0x9e3779b97f4a7c15ULL; return x ^ (x >> 29); }
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e9 + t.tv_nsec; }
int main(int argc, char **argv)
{
	const size_t max_entries = 100000, n = (size_t)(max_entries * atof(argv[1])), ops = atol(argv[2]);
	int fd = bpf_map_create(BPF_MAP_TYPE_HASH, "churn", 8, 8, max_entries, NULL);
	if (fd < 0) { perror("map"); return 1; }
	uint64_t *live = malloc(n * sizeof(*live)), v = 1, next = 0, rng = 88172645463325252ULL;
	for (size_t i = 0; i < n; i++) { live[i] = next++; uint64_t k = key(live[i]); if (bpf_map_update_elem(fd, &k, &v, BPF_ANY)) { perror("fill"); return 2; } }
	double t0 = now();
	for (size_t o = 0; o < ops; o++) {
		rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
		size_t s = rng % n; uint64_t k = key(live[s]);
		if (bpf_map_delete_elem(fd, &k)) { perror("delete"); return 3; }
		live[s] = next++; k = key(live[s]);
		if (bpf_map_update_elem(fd, &k, &v, BPF_NOEXIST)) { perror("insert"); return 4; }
	}
	printf("kernel load %s: %.1f ns per delete+insert (2 bpf() calls)\n", argv[1], (now() - t0) / ops);
	return 0;
}

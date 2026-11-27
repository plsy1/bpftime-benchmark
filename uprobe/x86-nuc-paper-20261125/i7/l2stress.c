// I7' L2 polluter: random pointer chase with a store per step over a BYTES-sized buffer (64-byte lines, one random
// cyclic permutation), forever. Run one per E core 9, 10, 11 (sharing the 2 MB L2 of cluster 8-11 with nginx on 8).
// Usage: l2stress BYTES. Prints "ready" after building the permutation.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

struct line { struct line *next; uint64_t pad[7]; };

int main(int argc, char **argv)
{
	size_t bytes = argc > 1 ? strtoull(argv[1], NULL, 10) : (1u << 20);
	size_t n = bytes / sizeof(struct line);
	struct line *a = aligned_alloc(64, n * sizeof(struct line));
	size_t *perm = malloc(n * sizeof(size_t));
	if (!a || !perm)
		return 1;
	for (size_t i = 0; i < n; i++)
		perm[i] = i;
	srand(12345);
	for (size_t i = n - 1; i > 0; i--) {
		size_t j = (size_t)rand() % (i + 1), t = perm[i];
		perm[i] = perm[j];
		perm[j] = t;
	}
	for (size_t i = 0; i < n; i++)
		a[perm[i]].next = &a[perm[(i + 1) % n]];
	free(perm);
	printf("ready %zu lines\n", n);
	fflush(stdout);
	struct line *p = &a[0];
	for (;;) {
		p->pad[0]++;
		p = p->next;
	}
}

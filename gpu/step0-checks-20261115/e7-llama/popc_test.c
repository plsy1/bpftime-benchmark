/* step0 e7-llama host unit test: E7_POPC32 and the leader election of
 * e7.bpf.c (exactly one lane of a non-empty mask is the lowest set bit),
 * checked against the compiler's builtins on edge cases and 2M random words. */
#include <stdint.h>
#include <stdio.h>
#include "e7_common.h"

static uint64_t rng = 88172645463325252ULL;
static uint64_t next(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

int main(void)
{
	long bad = 0, n = 0;
	uint64_t edge[] = { 0, 1, 0x80000000ULL, 0xffffffffULL, 0xffffULL, 0xffff0000ULL,
			    0x55555555ULL, 0xaaaaaaaaULL, 0x0f0f0f0fULL, 0xffffffff00000000ULL,
			    0xffffffffffffffffULL, 0x100000000ULL };
	for (unsigned i = 0; i < sizeof(edge) / sizeof(edge[0]) + 2000000; i++) {
		uint64_t x = i < sizeof(edge) / sizeof(edge[0]) ? edge[i] : next();
		uint64_t m = x & 0xffffffffULL;
		uint64_t got = E7_POPC32(x);
		uint64_t want = (uint64_t)__builtin_popcount((unsigned)m);
		if (got != want) { if (bad < 5) printf("popc mismatch x=%llx got=%llu want=%llu\n", (unsigned long long)x, (unsigned long long)got, (unsigned long long)want); bad++; }
		int leaders = 0;
		for (int lane = 0; lane < 32; lane++) {
			uint64_t self = 1ULL << lane;
			if (!(m & self)) continue; /* inactive lanes do not run the probe */
			if (E7_LOWBIT(m) == self) leaders++;
		}
		int want_leaders = m ? 1 : 0;
		if (leaders != want_leaders || (m && E7_LOWBIT(m) != (1ULL << __builtin_ctz((unsigned)m)))) {
			if (bad < 5) printf("leader mismatch m=%llx leaders=%d\n", (unsigned long long)m, leaders);
			bad++;
		}
		n++;
	}
	printf("popc_test: %ld masks checked, %ld mismatches\n", n, bad);
	return bad ? 1 : 0;
}

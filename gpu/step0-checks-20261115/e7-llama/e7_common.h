/* step0 e7-llama: layout of the histogram map and the bit tricks, shared by
 * the eBPF program (e7.bpf.c), the loader (e7_loader.c) and the host unit
 * test (popc_test.c). */
#ifndef E7_COMMON_H
#define E7_COMMON_H

/* Kernel slots. One object probes one kernel (E7_KIDX); the slots let one
 * object carry several programs (the optional all4 object). */
#define E7_K_RMS 0
#define E7_K_MMQ 1
#define E7_K_MMVQ 2
#define E7_K_ROPEK 3
#define E7_K_CTRL 4      /* control: 48-thread blocks (masks of 32 and 16) */
#define E7_K_CTRLDIV 5   /* control: odd lanes leave through their own ret,
			    even lanes wait ~200 us first (two calls of 16) */
#define E7_K_CTRLMERGE 6 /* reported: the same without the wait (do the two
			    exit groups merge inside the probe?) */
#define E7_NK 7

/* Per kernel slot: E7_NS copies (by smid % E7_NS, to spread the atomics over
 * 8 words) of E7_NB bins (bin k = calls whose active mask had k lanes; bin 0
 * is impossible and stays 0), then the extras. */
#define E7_NS 8
#define E7_NB 33
#define E7_HIST_WORDS (E7_NS * E7_NB) /* 264 */
#define E7_X_LAUNCHES (E7_HIST_WORDS + 0) /* thread (0,0,0) of block (0,0,0) at exit */
#define E7_X_ANOM_SELF (E7_HIST_WORDS + 1) /* a lane whose own bit is not in the mask */
#define E7_X_ANOM_ZERO (E7_HIST_WORDS + 2) /* a call that read an empty mask */
#define E7_STRIDE 272
#define E7_ENTRIES (E7_NK * E7_STRIDE) /* 1904 u64, 15,232 bytes */

#define E7_HIST_KEY(k, s, bin) ((k) * E7_STRIDE + (s) * E7_NB + (bin))
#define E7_X_KEY(k, x) ((k) * E7_STRIDE + (x))

/* Population count of the low 32 bits (SWAR; eBPF has no popcount). The
 * multiply leaves the byte sum in bits 24..31; higher product bits hold
 * partial sums and are masked off. */
#define E7_POPC32(x_in)                                                        \
	({                                                                     \
		unsigned long long __x = (x_in) & 0xffffffffULL;               \
		__x = __x - ((__x >> 1) & 0x55555555ULL);                      \
		__x = (__x & 0x33333333ULL) + ((__x >> 2) & 0x33333333ULL);    \
		__x = (__x + (__x >> 4)) & 0x0f0f0f0fULL;                      \
		((__x * 0x01010101ULL) >> 24) & 0xffULL;                       \
	})

/* The lowest set bit of m (m & -m), 0 for m == 0. */
#define E7_LOWBIT(m) ((m) & (~(m) + 1ULL))

#endif

// C6 (RULES-round2.md): message-passing litmus test for the hash map's publish order.
//
// Writer (one lane of a warp):  red.global.add.u64 [data_i], 1;  then flag_i = 1 in the publish form
// Reader (one lane of a warp in another block):
//                               spin ld.relaxed.gpu.global.b64 flag_i until 1 (bounded),
//                               then the acquire form, then ld.relaxed.gpu.global.b64 data_i.
// Weak outcome: flag 1 and data 0.
//
// -DVARIANT selects the forms (rules table):
//   A  publish st.release.gpu   reader fence.acq_rel.gpu
//   B  publish st.relaxed.gpu   reader fence.acq_rel.gpu
//   C  publish st.release.gpu   reader no fence
//   D  publish st.relaxed.gpu   reader no fence
// Harness checks, outside the rules table (writer order swapped: the flag before the add):
//   S  st.relaxed.gpu flag; red.global.add.u64 data;                       reader no fence
//   SD st.relaxed.gpu flag; a delay of SWAP_GAP cycles; red.global.add.u64; reader no fence
//
// Layout and scheduling:
// - data and flag cells live in two separate page-aligned allocations, one cell per 128-byte line.
//   Each launch assigns cells to instances with a fresh random permutation (data and flag apart).
// - Testing blocks: lane 0 of each warp is a writer or a reader (one instance at a time per warp).
//   Each launch pairs writer and reader warps at random, never in the same block.
// - A pair runs `rounds` instances back to back, each on fresh cells. Before round k the writer waits
//   until the reader is spinning on round k (the reader's `ready` word), then a seeded delay, then
//   the two writes.
// - Stress blocks hammer an unrelated 16 MiB region with red.add / st.relaxed / ld.relaxed on
//   random lines until every reader is done (bounded).
// - All blocks of a launch are resident at once (checked with the occupancy API).
// - %smid is read per instance by the writer (before its writes; stored after them) and by the
//   reader (before its first poll; stored with its record).
// - A second kernel (same stream) classifies every instance, checks the final data and flag words
//   (both must be 1), and zeroes the cells and records for the next launch.

#include <cuda_runtime.h>

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

typedef unsigned long long u64;
typedef unsigned int u32;

// ---------------------------------------------------------------- variant
#define C6_CAT2(a, b) a##b
#define C6_CAT(a, b) C6_CAT2(a, b)
#define C6_V_A 1
#define C6_V_B 2
#define C6_V_C 3
#define C6_V_D 4
#define C6_V_S 5
#define C6_V_SD 6
#ifndef VARIANT
#error "build with -DVARIANT=A|B|C|D|S|SD"
#endif
#define C6_V C6_CAT(C6_V_, VARIANT)
#define C6_STR2(x) #x
#define C6_STR(x) C6_STR2(x)

#if C6_V == C6_V_A
#define PUB_RELEASE 1
#define READER_FENCE 1
#define SWAPPED 0
#elif C6_V == C6_V_B
#define PUB_RELEASE 0
#define READER_FENCE 1
#define SWAPPED 0
#elif C6_V == C6_V_C
#define PUB_RELEASE 1
#define READER_FENCE 0
#define SWAPPED 0
#elif C6_V == C6_V_D
#define PUB_RELEASE 0
#define READER_FENCE 0
#define SWAPPED 0
#elif C6_V == C6_V_S
#define PUB_RELEASE 0
#define READER_FENCE 0
#define SWAPPED 1
#define SWAP_GAP 0
#elif C6_V == C6_V_SD
#define PUB_RELEASE 0
#define READER_FENCE 0
#define SWAPPED 1
#ifndef SWAP_GAP
#define SWAP_GAP 4096
#endif
#else
#error "unknown VARIANT"
#endif

// ---------------------------------------------------------------- constants
#define BLOCK 128
#define WARPS_PER_BLOCK (BLOCK / 32)
#define MIN_BLOCKS_PER_SM 12
#define CELL_BYTES 128ull
#define SPIN_CAP (1u << 20)       // reader polls of one flag before a timeout
#define WAIT_CAP (1u << 20)       // writer polls of `ready` before it goes on anyway
#define BARRIER_CAP (1u << 20)    // testing lanes' polls of the stress-arrival count
#define STRESS_MAX_OPS (1u << 22) // per stress thread per launch
#define STRESS_BYTES (16ull << 20)
#define MAXSM 16
#define WLOG_MAX 4096

enum { ST_NONE = 0, ST_SEEN = 1, ST_TIMEOUT = 2, ST_BADFLAG = 3 };

// device counters (cumulative over launches)
enum {
	K_INST_SAME, K_INST_DIFF,       // instances by writer/reader on the same / different SM
	K_STRONG_SAME, K_STRONG_DIFF,   // flag 1, data 1
	K_WEAK_SAME, K_WEAK_DIFF,       // flag 1, data 0
	K_TIMEOUT_SAME, K_TIMEOUT_DIFF, // flag never seen 1 within SPIN_CAP polls
	K_BADFLAG,                      // flag value other than 0 or 1
	K_BADDATA,                      // data value other than 0 or 1
	K_MISSING,                      // no reader record
	K_FINAL_BAD,                    // after the launch, data != 1 or flag != 1
	K_SAW0,                         // flag seen 1 after at least one poll that read 0
	K_FIRSTPOLL,                    // flag seen 1 on the first poll
	K_POLLS_SUM,                    // polls over flag-seen instances
	K_SMID_RANGE,                   // an smid >= MAXSM or a missing writer/reader smid
	K_WRITER_WAIT_TO,               // writer gave up waiting for `ready`
	K_BARRIER_TO,                   // testing lane gave up waiting for the stress blocks
	K_STRESS_CAP,                   // a stress thread hit STRESS_MAX_OPS
	K_STRESS_OPS,                   // stress operations issued (all stress threads, all launches)
	K_SAME_BLOCK,                   // writer and reader of a pair in the same block (must be 0)
	K_N
};
static const char *k_names[K_N] = {
	"inst_same_sm", "inst_diff_sm", "strong_same_sm", "strong_diff_sm", "weak_same_sm",
	"weak_diff_sm", "timeout_same_sm", "timeout_diff_sm", "bad_flag", "bad_data", "missing_record",
	"final_not_1", "saw0_then_1", "seen_on_first_poll", "polls_sum_seen", "smid_bad",
	"writer_wait_timeouts", "barrier_timeouts", "stress_hit_op_cap", "stress_ops", "pair_same_block"};

struct WeakEntry {
	u32 launch, pair, round, wsm, rsm, wblock, rblock, dcell, fcell, polls;
};

struct Params {
	u64 data, flag, stress, ctl; // ctl: [0] stress arrivals, [16] readers done, ready[p] at 32 + 16 p (u64 index)
	const u32 *role;             // per testing warp: pair << 1 | is_reader, or ~0u
	const u32 *dcell, *fcell;    // per instance (pair * rounds + round)
	u64 *rec;                    // per instance: reader record
	u32 *wsm_i, *rsm_i;          // per instance: %smid of the writer and of the reader
	u32 *pinfo;                  // per pair: unused, unused, wblock, rblock
	u64 *cnt;                    // counters
	u64 seed;
	u32 launch, n_test_blocks, n_test_warps, stress_threads, n_pairs, rounds, delay_max;
	u64 stress_lines;
};

// ---------------------------------------------------------------- PTX helpers (forms as in C4)
__device__ __forceinline__ u64 to_global(u64 p)
{
	u64 g;
	asm("cvta.to.global.u64 %0, %1;" : "=l"(g) : "l"(p));
	return g;
}
__device__ __forceinline__ u64 ld_relaxed(u64 a)
{
	u64 v;
	asm volatile("ld.relaxed.gpu.global.b64 %0, [%1];" : "=l"(v) : "l"(a) : "memory");
	return v;
}
__device__ __forceinline__ void fence_acq_rel()
{
	asm volatile("fence.acq_rel.gpu;" ::: "memory");
}
__device__ __forceinline__ void red_add(u64 a, u64 v)
{
	asm volatile("red.global.add.u64 [%0], %1;" ::"l"(a), "l"(v) : "memory");
}
__device__ __forceinline__ void st_release(u64 a, u64 v)
{
	asm volatile("st.release.gpu.global.b64 [%0], %1;" ::"l"(a), "l"(v) : "memory");
}
__device__ __forceinline__ void st_relaxed(u64 a, u64 v)
{
	asm volatile("st.relaxed.gpu.global.b64 [%0], %1;" ::"l"(a), "l"(v) : "memory");
}
__device__ __forceinline__ u32 smid()
{
	u32 r;
	asm volatile("mov.u32 %0, %%smid;" : "=r"(r));
	return r;
}
__device__ __forceinline__ void pin(u64 &x)
{
	asm volatile("" : "+l"(x)); // keeps an address computed before the next asm
}
__device__ __forceinline__ void delay_cycles(u32 c)
{
	if (c == 0)
		return;
	const long long t0 = clock64();
	while ((u64)(clock64() - t0) < (u64)c) {
	}
}
__host__ __device__ __forceinline__ u64 mix64(u64 x)
{
	x += 0x9E3779B97F4A7C15ull;
	x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
	x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
	return x ^ (x >> 31);
}

// ---------------------------------------------------------------- the publish under test
__device__ __forceinline__ void publish(u64 fa)
{
#if PUB_RELEASE
	st_release(fa, 1);
#else
	st_relaxed(fa, 1);
#endif
}

__device__ __forceinline__ void writer(const Params &P, u32 pair)
{
	const u64 ready = to_global(P.ctl + 8ull * (32ull + 16ull * pair));
	const u64 dbase = to_global(P.data), fbase = to_global(P.flag);
	u32 wait_to = 0;
	for (u32 k = 0; k < P.rounds; ++k) {
		const u32 inst = pair * P.rounds + k;
		u64 da = dbase + (u64)P.dcell[inst] * CELL_BYTES;
		u64 fa = fbase + (u64)P.fcell[inst] * CELL_BYTES;
		u32 d = (u32)(mix64(P.seed ^ ((u64)P.launch << 40) ^ inst) % P.delay_max);
		pin(da);
		pin(fa);
		const u32 sm = smid();
		// wait until the reader spins on round k (it stores k + 1 before its first poll)
		u32 polls = 0;
		while (ld_relaxed(ready) < (u64)k + 1) {
			if (++polls >= WAIT_CAP) {
				++wait_to;
				break;
			}
		}
		delay_cycles(d);
#if !SWAPPED
		red_add(da, 1);
		publish(fa);
#else
		st_relaxed(fa, 1);
#if SWAP_GAP > 0
		delay_cycles(SWAP_GAP);
#endif
		red_add(da, 1);
#endif
		P.wsm_i[inst] = sm;
	}
	P.pinfo[4 * pair + 2] = blockIdx.x;
	if (wait_to)
		atomicAdd(&P.cnt[K_WRITER_WAIT_TO], (u64)wait_to);
}

__device__ __forceinline__ void reader(const Params &P, u32 pair)
{
	const u64 ready = to_global(P.ctl + 8ull * (32ull + 16ull * pair));
	const u64 done = to_global(P.ctl + 8ull * 16ull);
	const u64 dbase = to_global(P.data), fbase = to_global(P.flag);
	for (u32 k = 0; k < P.rounds; ++k) {
		const u32 inst = pair * P.rounds + k;
		u64 da = dbase + (u64)P.dcell[inst] * CELL_BYTES;
		u64 fa = fbase + (u64)P.fcell[inst] * CELL_BYTES;
		pin(da);
		pin(fa);
		const u32 sm = smid();
		st_relaxed(ready, (u64)k + 1);
		u64 f;
		u32 polls = 0;
		do {
			f = ld_relaxed(fa);
			++polls;
		} while (f == 0 && polls < SPIN_CAP);
		u64 v = 0xFFFFFFull;
		u32 st;
		if (f == 1) {
#if READER_FENCE
			fence_acq_rel();
#endif
			v = ld_relaxed(da);
			st = ST_SEEN;
		} else if (f == 0) {
			st = ST_TIMEOUT;
		} else {
			st = ST_BADFLAG;
		}
		P.rec[inst] = ((u64)polls << 32) | ((u64)st << 24) | (v > 0xFFFFFFull ? 0xFFFFFFull : v);
		P.rsm_i[inst] = sm;
	}
	P.pinfo[4 * pair + 3] = blockIdx.x;
	red_add(done, 1);
}

__device__ __forceinline__ void stress(const Params &P)
{
	const u64 arrive = to_global(P.ctl);
	const u64 done = to_global(P.ctl + 8ull * 16ull);
	const u64 base = to_global(P.stress);
	const u32 t = (blockIdx.x - P.n_test_blocks) * BLOCK + threadIdx.x;
	red_add(arrive, 1);
	u64 x = mix64(P.seed ^ ((u64)P.launch << 32) ^ ((u64)t << 8) ^ 0x5157ull);
	u64 acc = 0;
	u32 it = 0;
	for (; it < STRESS_MAX_OPS; ++it) {
		if ((it & 63u) == 0 && ld_relaxed(done) >= P.n_pairs)
			break;
		x ^= x << 13;
		x ^= x >> 7;
		x ^= x << 17;
		const u64 a = base + ((x >> 20) & (P.stress_lines - 1)) * CELL_BYTES + ((x >> 4) & 15ull) * 8ull;
		switch (x & 3ull) {
		case 0:
		case 1:
			red_add(a, 1);
			break;
		case 2:
			st_relaxed(a, x);
			break;
		default:
			acc += ld_relaxed(a);
		}
	}
	if (it >= STRESS_MAX_OPS)
		atomicAdd(&P.cnt[K_STRESS_CAP], 1ull);
	atomicAdd(&P.cnt[K_STRESS_OPS], (u64)it);
	if (acc == 0x5157ull) // never true in practice; keeps the loads' results live
		atomicAdd(&P.cnt[K_STRESS_CAP], 0ull);
}

__global__ void __launch_bounds__(BLOCK, MIN_BLOCKS_PER_SM) c6_kernel(Params P)
{
	if (blockIdx.x >= P.n_test_blocks) {
		stress(P);
		return;
	}
	if ((threadIdx.x & 31u) != 0)
		return;
	const u32 w = blockIdx.x * WARPS_PER_BLOCK + (threadIdx.x >> 5);
	const u32 role = P.role[w];
	if (role == 0xFFFFFFFFu)
		return;
	// start once every stress thread is running (bounded)
	const u64 arrive = to_global(P.ctl);
	u32 polls = 0;
	while (ld_relaxed(arrive) < (u64)P.stress_threads) {
		if (++polls >= BARRIER_CAP) {
			atomicAdd(&P.cnt[K_BARRIER_TO], 1ull);
			break;
		}
	}
	if (role & 1u)
		reader(P, role >> 1);
	else
		writer(P, role >> 1);
}

// ---------------------------------------------------------------- tally and reset
struct TParams {
	u64 *data, *flag, *rec;
	const u32 *dcell, *fcell, *pinfo;
	u32 *wsm_i, *rsm_i;
	u64 *cnt, *mat_inst, *mat_weak; // mat[wsm * MAXSM + rsm]
	WeakEntry *wlog;
	u32 *wlog_n;
	u32 n_inst, rounds, launch;
};

__global__ void c6_tally(TParams T)
{
	__shared__ u32 sc[K_N];
	__shared__ u32 smi[MAXSM * MAXSM], smw[MAXSM * MAXSM];
	__shared__ unsigned long long spolls;
	for (u32 i = threadIdx.x; i < K_N; i += blockDim.x)
		sc[i] = 0;
	for (u32 i = threadIdx.x; i < MAXSM * MAXSM; i += blockDim.x)
		smi[i] = smw[i] = 0;
	if (threadIdx.x == 0)
		spolls = 0;
	__syncthreads();
	const u32 inst = blockIdx.x * blockDim.x + threadIdx.x;
	if (inst < T.n_inst) {
		const u32 pair = inst / T.rounds;
		const u64 r = T.rec[inst];
		const u32 st = (u32)(r >> 24) & 0xFFu;
		const u32 v = (u32)(r & 0xFFFFFFull);
		const u32 polls = (u32)(r >> 32);
		const u32 wsm = T.wsm_i[inst], rsm = T.rsm_i[inst];
		if (inst % T.rounds == 0 && T.pinfo[4 * pair + 2] == T.pinfo[4 * pair + 3])
			atomicAdd(&sc[K_SAME_BLOCK], 1u);
		const bool smok = wsm < MAXSM && rsm < MAXSM;
		const bool same = wsm == rsm;
		const u32 dc = T.dcell[inst], fc = T.fcell[inst];
		const u64 dfin = T.data[dc * (CELL_BYTES / 8)], ffin = T.flag[fc * (CELL_BYTES / 8)];
		if (!smok)
			atomicAdd(&sc[K_SMID_RANGE], 1u);
		atomicAdd(&sc[same ? K_INST_SAME : K_INST_DIFF], 1u);
		if (smok)
			atomicAdd(&smi[wsm * MAXSM + rsm], 1u);
		if (st == ST_SEEN) {
			if (v == 0) {
				atomicAdd(&sc[same ? K_WEAK_SAME : K_WEAK_DIFF], 1u);
				if (smok)
					atomicAdd(&smw[wsm * MAXSM + rsm], 1u);
				const u32 j = atomicAdd(T.wlog_n, 1u);
				if (j < WLOG_MAX) {
					WeakEntry e;
					e.launch = T.launch;
					e.pair = pair;
					e.round = inst % T.rounds;
					e.wsm = wsm;
					e.rsm = rsm;
					e.wblock = T.pinfo[4 * pair + 2];
					e.rblock = T.pinfo[4 * pair + 3];
					e.dcell = dc;
					e.fcell = fc;
					e.polls = polls;
					T.wlog[j] = e;
				}
			} else if (v == 1) {
				atomicAdd(&sc[same ? K_STRONG_SAME : K_STRONG_DIFF], 1u);
			} else {
				atomicAdd(&sc[K_BADDATA], 1u);
			}
			atomicAdd(&sc[polls > 1 ? K_SAW0 : K_FIRSTPOLL], 1u);
			atomicAdd(&spolls, (unsigned long long)polls);
		} else if (st == ST_TIMEOUT) {
			atomicAdd(&sc[same ? K_TIMEOUT_SAME : K_TIMEOUT_DIFF], 1u);
		} else if (st == ST_BADFLAG) {
			atomicAdd(&sc[K_BADFLAG], 1u);
		} else {
			atomicAdd(&sc[K_MISSING], 1u);
		}
		if (dfin != 1 || ffin != 1)
			atomicAdd(&sc[K_FINAL_BAD], 1u);
		T.data[dc * (CELL_BYTES / 8)] = 0;
		T.flag[fc * (CELL_BYTES / 8)] = 0;
		T.rec[inst] = 0;
		T.wsm_i[inst] = 0xFFFFFFFFu;
		T.rsm_i[inst] = 0xFFFFFFFFu;
	}
	__syncthreads();
	for (u32 i = threadIdx.x; i < K_N; i += blockDim.x)
		if (sc[i])
			atomicAdd((unsigned long long *)&T.cnt[i], (u64)sc[i]);
	for (u32 i = threadIdx.x; i < MAXSM * MAXSM; i += blockDim.x) {
		if (smi[i])
			atomicAdd((unsigned long long *)&T.mat_inst[i], (u64)smi[i]);
		if (smw[i])
			atomicAdd((unsigned long long *)&T.mat_weak[i], (u64)smw[i]);
	}
	if (threadIdx.x == 0 && spolls)
		atomicAdd((unsigned long long *)&T.cnt[K_POLLS_SUM], (u64)spolls);
}

// ---------------------------------------------------------------- host
#define CK(x)                                                                                        \
	do {                                                                                         \
		cudaError_t e_ = (x);                                                                \
		if (e_ != cudaSuccess) {                                                             \
			fprintf(stderr, "CUDA error %s at %s:%d: %s\n", cudaGetErrorName(e_), __FILE__, \
				__LINE__, cudaGetErrorString(e_));                                   \
			exit(4);                                                                     \
		}                                                                                    \
	} while (0)

static double now_s()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static void usage()
{
	fprintf(stderr, "usage: c6_X [--target N] [--seconds S] [--rounds R] [--seed X] [--delay-max C]\n"
			"            [--test-blocks N] [--stress-blocks N] [--progress S]\n");
	exit(4);
}

int main(int argc, char **argv)
{
	u64 target = 100000000ull; // 10^8 instances
	double seconds = 600.0;    // or 10 minutes, whichever first
	u32 rounds = 256, delay_max = 1024, test_blocks = 0, stress_blocks = 0;
	u64 seed = 0xC6C6ull;
	double progress = 15.0;
	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		if (i + 1 >= argc)
			usage();
		const char *v = argv[++i];
		if (a == "--target")
			target = strtoull(v, nullptr, 0);
		else if (a == "--seconds")
			seconds = atof(v);
		else if (a == "--rounds")
			rounds = (u32)strtoul(v, nullptr, 0);
		else if (a == "--seed")
			seed = strtoull(v, nullptr, 0);
		else if (a == "--delay-max")
			delay_max = (u32)strtoul(v, nullptr, 0);
		else if (a == "--test-blocks")
			test_blocks = (u32)strtoul(v, nullptr, 0);
		else if (a == "--stress-blocks")
			stress_blocks = (u32)strtoul(v, nullptr, 0);
		else if (a == "--progress")
			progress = atof(v);
		else
			usage();
	}
	if (delay_max == 0)
		delay_max = 1;

	cudaDeviceProp prop;
	CK(cudaGetDeviceProperties(&prop, 0));
	int occ = 0;
	CK(cudaOccupancyMaxActiveBlocksPerMultiprocessor(&occ, c6_kernel, BLOCK, 0));
	const u32 resident = (u32)occ * (u32)prop.multiProcessorCount;
	if (test_blocks == 0 && stress_blocks == 0) {
		stress_blocks = resident / 3;
		test_blocks = resident - stress_blocks;
	}
	cudaFuncAttributes fa;
	CK(cudaFuncGetAttributes(&fa, c6_kernel));
	printf("C6 variant=%s publish=%s reader=%s order=%s\n", C6_STR(VARIANT),
	       PUB_RELEASE ? "st.release.gpu.global.b64" : "st.relaxed.gpu.global.b64",
	       READER_FENCE ? "fence.acq_rel.gpu" : "none",
#if SWAPPED
	       SWAP_GAP ? "swapped(flag, delay, add)" : "swapped(flag, add)"
#else
	       "add-then-flag"
#endif
	);
	printf("device=\"%s\" sm_%d%d SMs=%d l2=%d B\n", prop.name, prop.major, prop.minor,
	       prop.multiProcessorCount, prop.l2CacheSize);
	printf("kernel regs=%d local=%zu B; occupancy=%d blocks/SM x %d SMs = %u resident blocks of %d threads\n",
	       fa.numRegs, (size_t)fa.localSizeBytes, occ, prop.multiProcessorCount, resident, BLOCK);
	if (test_blocks + stress_blocks > resident || test_blocks < 2) {
		printf("ERROR: %u test + %u stress blocks do not fit in %u resident blocks\n", test_blocks,
		       stress_blocks, resident);
		return 4;
	}
	const u32 n_test_warps = test_blocks * WARPS_PER_BLOCK;
	const u32 n_pairs = n_test_warps / 2;
	const u32 stress_threads = stress_blocks * BLOCK;
	const u64 n_cells = (u64)n_pairs * rounds;
	const u64 stress_lines = STRESS_BYTES / CELL_BYTES;
	printf("params: target=%llu instances, time limit=%.0f s, test blocks=%u (%u warps, %u pairs, lane 0 "
	       "only), stress blocks=%u (%u threads), rounds/launch=%u (%llu instances/launch), "
	       "writer delay=[0,%u) cycles, seed=0x%llx\n",
	       target, seconds, test_blocks, n_test_warps, n_pairs, stress_blocks, stress_threads, rounds,
	       n_cells, delay_max, seed);
#if SWAPPED
	printf("params: swap gap=%u cycles\n", (u32)SWAP_GAP);
#endif
	printf("caps: reader spin %u polls, writer wait %u polls, barrier %u polls, stress %u ops\n", SPIN_CAP,
	       WAIT_CAP, BARRIER_CAP, STRESS_MAX_OPS);

	// allocations: data and flag apart, page-aligned, page-multiple sizes
	const size_t region = (size_t)(n_cells * CELL_BYTES);
	const size_t region_pg = (region + 4095) & ~(size_t)4095;
	u64 *d_data, *d_flag, *d_stress, *d_ctl, *d_rec, *d_cnt, *d_mi, *d_mw;
	u32 *d_role, *d_dcell, *d_fcell, *d_pinfo, *d_wlog_n, *d_wsm, *d_rsm;
	WeakEntry *d_wlog;
	const size_t ctl_bytes = 8ull * (32ull + 16ull * n_pairs);
	CK(cudaMalloc(&d_data, region_pg));
	CK(cudaMalloc(&d_flag, region_pg));
	CK(cudaMalloc(&d_stress, STRESS_BYTES));
	CK(cudaMalloc(&d_ctl, ctl_bytes));
	CK(cudaMalloc(&d_rec, 8 * n_cells));
	CK(cudaMalloc(&d_cnt, 8 * K_N));
	CK(cudaMalloc(&d_mi, 8 * MAXSM * MAXSM));
	CK(cudaMalloc(&d_mw, 8 * MAXSM * MAXSM));
	CK(cudaMalloc(&d_role, 4 * n_test_warps));
	CK(cudaMalloc(&d_dcell, 4 * n_cells));
	CK(cudaMalloc(&d_fcell, 4 * n_cells));
	CK(cudaMalloc(&d_pinfo, 16 * n_pairs));
	CK(cudaMalloc(&d_wsm, 4 * n_cells));
	CK(cudaMalloc(&d_rsm, 4 * n_cells));
	CK(cudaMemset(d_wsm, 0xFF, 4 * n_cells));
	CK(cudaMemset(d_rsm, 0xFF, 4 * n_cells));
	CK(cudaMalloc(&d_wlog_n, 4));
	CK(cudaMalloc(&d_wlog, sizeof(WeakEntry) * WLOG_MAX));
	const bool aligned = ((uintptr_t)d_data % 4096 == 0) && ((uintptr_t)d_flag % 4096 == 0);
	const uintptr_t lo = std::min((uintptr_t)d_data, (uintptr_t)d_flag),
			hi = std::max((uintptr_t)d_data, (uintptr_t)d_flag);
	printf("layout: data=%p flag=%p (%zu B each, page-aligned=%s, disjoint=%s), cell stride %llu B, "
	       "stress=%p (%llu B, %llu lines), ctl %zu B\n",
	       (void *)d_data, (void *)d_flag, region_pg, aligned ? "yes" : "NO",
	       (lo + region_pg <= hi) ? "yes" : "NO", CELL_BYTES, (void *)d_stress, STRESS_BYTES,
	       stress_lines, ctl_bytes);
	if (!aligned || lo + region_pg > hi) {
		printf("ERROR: data and flag regions are not page-separated\n");
		return 4;
	}
	CK(cudaMemset(d_data, 0, region_pg));
	CK(cudaMemset(d_flag, 0, region_pg));
	CK(cudaMemset(d_stress, 0, STRESS_BYTES));
	CK(cudaMemset(d_rec, 0, 8 * n_cells));
	CK(cudaMemset(d_cnt, 0, 8 * K_N));
	CK(cudaMemset(d_mi, 0, 8 * MAXSM * MAXSM));
	CK(cudaMemset(d_mw, 0, 8 * MAXSM * MAXSM));
	CK(cudaMemset(d_wlog_n, 0, 4));

	u32 *h_role, *h_dcell, *h_fcell;
	u64 *h_cnt;
	CK(cudaMallocHost(&h_role, 4 * n_test_warps));
	CK(cudaMallocHost(&h_dcell, 4 * n_cells));
	CK(cudaMallocHost(&h_fcell, 4 * n_cells));
	CK(cudaMallocHost(&h_cnt, 8 * K_N));
	std::vector<u32> wperm(n_test_warps), dperm(n_cells), fperm(n_cells);
	for (u32 i = 0; i < n_test_warps; ++i)
		wperm[i] = i;
	for (u64 i = 0; i < n_cells; ++i)
		dperm[i] = fperm[i] = (u32)i;
	std::mt19937_64 rng(seed);
	cudaStream_t s;
	CK(cudaStreamCreateWithFlags(&s, cudaStreamNonBlocking));

	u64 prev[K_N] = {0};
	u64 inst_total = 0, launches = 0, rounds_total = 0;
	int err = 0;
	const double t0 = now_s();
	double next_prog = t0 + progress;
	const u64 rounds_needed = (target + n_pairs - 1) / n_pairs; // per pair
	while (rounds_total < rounds_needed && now_s() - t0 < seconds) {
		const u32 r_now = (u32)std::min<u64>(rounds, rounds_needed - rounds_total);
		const u64 n_inst = (u64)n_pairs * r_now;
		// thread randomization: pair warps at random, writer and reader in different blocks
		std::shuffle(wperm.begin(), wperm.end(), rng);
		for (u32 p = 0; p < n_pairs; ++p) {
			while (wperm[2 * p] / WARPS_PER_BLOCK == wperm[2 * p + 1] / WARPS_PER_BLOCK) {
				const u32 q = (u32)(rng() % n_pairs);
				if (q == p)
					continue;
				if (wperm[2 * q + 1] / WARPS_PER_BLOCK != wperm[2 * p] / WARPS_PER_BLOCK &&
				    wperm[2 * p + 1] / WARPS_PER_BLOCK != wperm[2 * q] / WARPS_PER_BLOCK)
					std::swap(wperm[2 * p + 1], wperm[2 * q + 1]);
			}
		}
		for (u32 i = 0; i < n_test_warps; ++i)
			h_role[i] = 0xFFFFFFFFu;
		for (u32 p = 0; p < n_pairs; ++p) {
			h_role[wperm[2 * p]] = p << 1;
			h_role[wperm[2 * p + 1]] = (p << 1) | 1u;
		}
		// location randomization: fresh cells per instance, data and flag permuted apart
		for (u64 i = 0; i < n_inst; ++i) {
			std::swap(dperm[i], dperm[i + rng() % (n_cells - i)]);
			std::swap(fperm[i], fperm[i + rng() % (n_cells - i)]);
		}
		memcpy(h_dcell, dperm.data(), 4 * n_inst);
		memcpy(h_fcell, fperm.data(), 4 * n_inst);
		CK(cudaMemcpyAsync(d_role, h_role, 4 * n_test_warps, cudaMemcpyHostToDevice, s));
		CK(cudaMemcpyAsync(d_dcell, h_dcell, 4 * n_inst, cudaMemcpyHostToDevice, s));
		CK(cudaMemcpyAsync(d_fcell, h_fcell, 4 * n_inst, cudaMemcpyHostToDevice, s));
		CK(cudaMemsetAsync(d_ctl, 0, ctl_bytes, s));
		CK(cudaMemsetAsync(d_pinfo, 0xFF, 16 * n_pairs, s));
		Params P;
		P.data = (u64)d_data;
		P.flag = (u64)d_flag;
		P.stress = (u64)d_stress;
		P.ctl = (u64)d_ctl;
		P.role = d_role;
		P.dcell = d_dcell;
		P.fcell = d_fcell;
		P.rec = d_rec;
		P.pinfo = d_pinfo;
		P.wsm_i = d_wsm;
		P.rsm_i = d_rsm;
		P.cnt = d_cnt;
		P.seed = seed;
		P.launch = (u32)launches;
		P.n_test_blocks = test_blocks;
		P.n_test_warps = n_test_warps;
		P.stress_threads = stress_threads;
		P.n_pairs = n_pairs;
		P.rounds = r_now;
		P.delay_max = delay_max;
		P.stress_lines = stress_lines;
		c6_kernel<<<test_blocks + stress_blocks, BLOCK, 0, s>>>(P);
		CK(cudaGetLastError());
		TParams T;
		T.data = d_data;
		T.flag = d_flag;
		T.rec = d_rec;
		T.dcell = d_dcell;
		T.fcell = d_fcell;
		T.pinfo = d_pinfo;
		T.wsm_i = d_wsm;
		T.rsm_i = d_rsm;
		T.cnt = d_cnt;
		T.mat_inst = d_mi;
		T.mat_weak = d_mw;
		T.wlog = d_wlog;
		T.wlog_n = d_wlog_n;
		T.n_inst = (u32)n_inst;
		T.rounds = r_now;
		T.launch = (u32)launches;
		c6_tally<<<(u32)((n_inst + 255) / 256), 256, 0, s>>>(T);
		CK(cudaGetLastError());
		CK(cudaMemcpyAsync(h_cnt, d_cnt, 8 * K_N, cudaMemcpyDeviceToHost, s));
		CK(cudaStreamSynchronize(s));
		++launches;
		rounds_total += r_now;
		inst_total += n_inst;
		// per-launch exact accounting
		u64 dl[K_N];
		for (int i = 0; i < K_N; ++i)
			dl[i] = h_cnt[i] - prev[i];
		const u64 classified = dl[K_STRONG_SAME] + dl[K_STRONG_DIFF] + dl[K_WEAK_SAME] + dl[K_WEAK_DIFF] +
				       dl[K_TIMEOUT_SAME] + dl[K_TIMEOUT_DIFF] + dl[K_BADFLAG] + dl[K_MISSING];
		const u64 by_sm = dl[K_INST_SAME] + dl[K_INST_DIFF];
		if (classified != n_inst || by_sm != n_inst || dl[K_FINAL_BAD] || dl[K_MISSING] || dl[K_BADFLAG] ||
		    dl[K_BADDATA] || dl[K_SMID_RANGE] || dl[K_SAME_BLOCK]) {
			printf("ERROR launch %llu: instances %llu, classified %llu, by SM %llu, final_not_1 %llu, "
			       "missing %llu, bad_flag %llu, bad_data %llu, smid_bad %llu, pair_same_block %llu\n",
			       launches - 1, n_inst, classified, by_sm, dl[K_FINAL_BAD], dl[K_MISSING],
			       dl[K_BADFLAG], dl[K_BADDATA], dl[K_SMID_RANGE], dl[K_SAME_BLOCK]);
			err = 1;
		}
		memcpy(prev, h_cnt, sizeof(prev));
		if (err)
			break;
		if (now_s() >= next_prog) {
			next_prog += progress;
			printf("progress t=%.1f s launches=%llu instances=%llu weak=%llu+%llu timeouts=%llu+%llu "
			       "saw0_then_1=%llu\n",
			       now_s() - t0, launches, inst_total, h_cnt[K_WEAK_SAME], h_cnt[K_WEAK_DIFF],
			       h_cnt[K_TIMEOUT_SAME], h_cnt[K_TIMEOUT_DIFF], h_cnt[K_SAW0]);
			fflush(stdout);
		}
	}
	const double wall = now_s() - t0;

	// after the last reset every data and flag word must be 0
	std::vector<u64> hd(region_pg / 8), hf(region_pg / 8);
	CK(cudaMemcpy(hd.data(), d_data, region_pg, cudaMemcpyDeviceToHost));
	CK(cudaMemcpy(hf.data(), d_flag, region_pg, cudaMemcpyDeviceToHost));
	u64 nz = 0;
	for (size_t i = 0; i < hd.size(); ++i)
		nz += (hd[i] != 0) + (hf[i] != 0);
	u64 mi[MAXSM * MAXSM], mw[MAXSM * MAXSM];
	u32 wn = 0;
	CK(cudaMemcpy(mi, d_mi, sizeof(mi), cudaMemcpyDeviceToHost));
	CK(cudaMemcpy(mw, d_mw, sizeof(mw), cudaMemcpyDeviceToHost));
	CK(cudaMemcpy(&wn, d_wlog_n, 4, cudaMemcpyDeviceToHost));
	std::vector<WeakEntry> wl(std::min<u32>(wn, WLOG_MAX));
	if (!wl.empty())
		CK(cudaMemcpy(wl.data(), d_wlog, sizeof(WeakEntry) * wl.size(), cudaMemcpyDeviceToHost));

	const u64 *c = h_cnt;
	const u64 weak = c[K_WEAK_SAME] + c[K_WEAK_DIFF];
	const u64 seen = c[K_STRONG_SAME] + c[K_STRONG_DIFF] + weak;
	const u64 tmo = c[K_TIMEOUT_SAME] + c[K_TIMEOUT_DIFF];
	printf("=== summary variant=%s ===\n", C6_STR(VARIANT));
	printf("stop: %s (instances %llu of target %llu, wall %.1f s of %.0f s)\n",
	       err ? "harness error" : (rounds_total >= rounds_needed ? "instance target" : "time limit"),
	       inst_total, target, wall, seconds);
	printf("launches=%llu instances=%llu wall_s=%.2f\n", launches, inst_total, wall);
	for (int i = 0; i < K_N; ++i)
		printf("counter %-22s %llu\n", k_names[i], c[i]);
	printf("instances: launched=%llu flag_seen=%llu timeouts=%llu (same-SM %llu, diff-SM %llu)\n", inst_total,
	       seen, tmo, c[K_TIMEOUT_SAME], c[K_TIMEOUT_DIFF]);
	printf("by SM: same-SM instances=%llu weak=%llu | diff-SM instances=%llu weak=%llu\n", c[K_INST_SAME],
	       c[K_WEAK_SAME], c[K_INST_DIFF], c[K_WEAK_DIFF]);
	printf("mean polls per flag-seen instance=%.2f\n", seen ? (double)c[K_POLLS_SUM] / (double)seen : 0.0);
	printf("final scan: nonzero data/flag words after the last reset=%llu\n", nz);
	printf("SM pair matrix (row writer SM, column reader SM): instances / weak\n");
	for (int w = 0; w < MAXSM; ++w) {
		bool any = false;
		for (int r = 0; r < MAXSM; ++r)
			any |= mi[w * MAXSM + r] != 0;
		if (!any)
			continue;
		printf("  w%-2d", w);
		for (int r = 0; r < MAXSM; ++r) {
			bool col = false;
			for (int ww = 0; ww < MAXSM; ++ww)
				col |= mi[ww * MAXSM + r] != 0;
			if (col)
				printf(" r%d:%llu/%llu", r, mi[w * MAXSM + r], mw[w * MAXSM + r]);
		}
		printf("\n");
	}
	printf("weak outcomes logged=%u (first %zu kept)\n", wn, wl.size());
	for (size_t i = 0; i < wl.size() && i < 40; ++i)
		printf("  weak launch=%u pair=%u round=%u wsm=%u rsm=%u wblock=%u rblock=%u dcell=%u fcell=%u "
		       "polls=%u\n",
		       wl[i].launch, wl[i].pair, wl[i].round, wl[i].wsm, wl[i].rsm, wl[i].wblock, wl[i].rblock,
		       wl[i].dcell, wl[i].fcell, wl[i].polls);
	if (nz)
		err = 1;
	int rc = 0;
	if (err)
		rc = 3;
	else if (C6_V == C6_V_A && weak > 0)
		rc = 2;
	printf("RESULT variant=%s instances=%llu flag_seen=%llu weak=%llu (same-SM %llu, diff-SM %llu) "
	       "timeouts=%llu wall_s=%.2f harness_error=%d exit=%d\n",
	       C6_STR(VARIANT), inst_total, seen, weak, c[K_WEAK_SAME], c[K_WEAK_DIFF], tmo, wall, err, rc);
	return rc;
}

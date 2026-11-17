// probe_shape: what does the shape of an entry/exit latency probe cost per
// thread execution, on an empty kernel and on a memory-bound one? Design
// round after the prototype (proto/map-model, 2026-11-17). Built from
// merge-cost/merge_cost.cu (same gate, calibration, rounds, validation).
//
// Probe shapes (the tool: entry stores %globaltimer, exit takes
// bucket = 63 - clz(delta | 1) and adds 1 to it):
//   none      no probes
//   c_empty   __noinline__ entry and exit that do nothing (call floor)
//   c_carry   __noinline__, start in a kernel-scope local carry passed by
//             generic address (the prototype's structure), no merge
//   c_smlane  c_carry + red.add to hist[bucket][smid][lane]
//   c_param   __noinline__, the entry returns the start, the exit takes it
//             as a parameter (no local memory), + the (smid, lane) add
//   r_carry   inline, start in a register, no merge
//   r_smlane  r_carry + the (smid, lane) add
//   r_warp    r_carry + lanes of the active mask with the same bucket
//             combined by match.any; the lowest such lane adds popc
// Bodies: empty, or mem (each thread reads 16 bytes of a 64 MB buffer and
// sums them: memory-bound, about the bytes per thread of mul_mat_vec_q).
// Output: OUT/raw.csv (round, kernel, shape, threads, n, ms, capped).
#include <cuda_runtime.h>
#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#define CK(x)                                                                  \
	do {                                                                   \
		cudaError_t e_ = (x);                                          \
		if (e_ != cudaSuccess) {                                       \
			fprintf(stderr, "%s:%d %s: %s\n", __FILE__, __LINE__, \
				#x, cudaGetErrorString(e_));                   \
			exit(2);                                               \
		}                                                              \
	} while (0)

static constexpr int NB = 64;
static constexpr int NSM_MAX = 64;
static constexpr size_t HIST_WORDS = (size_t)NB * NSM_MAX * 32;
static constexpr size_t SRC_N = (size_t)1 << 22; // uint4 elements: 64 MB
// a bucket no thread reaches, passed at launch so the compiler keeps the
// bucket (and its timer reads) of the shapes that do not merge
static constexpr unsigned NEVER = 999;

__device__ __forceinline__ uint64_t gtimer()
{
	uint64_t t;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	return t;
}
__device__ __forceinline__ unsigned smid()
{
	unsigned r;
	asm volatile("mov.u32 %0, %%smid;" : "=r"(r));
	return r;
}
__device__ __forceinline__ unsigned laneid()
{
	unsigned r;
	asm volatile("mov.u32 %0, %%laneid;" : "=r"(r));
	return r;
}
__device__ __forceinline__ void red_add(uint64_t *p, uint64_t v)
{
	asm volatile("red.global.add.u64 [%0], %1;" ::"l"(p), "l"(v)
		     : "memory");
}
__device__ __forceinline__ unsigned bucket(uint64_t t0)
{
	uint64_t d = gtimer() - t0;
	return 63 - __clzll(d | 1);
}
__device__ __forceinline__ uint64_t *smlane_word(uint64_t *hist, unsigned b,
						 unsigned lane)
{
	return hist + ((size_t)b * NSM_MAX + (smid() & (NSM_MAX - 1))) * 32 +
	       lane;
}

// call-shaped probes
__device__ __noinline__ void c_entry_empty() { asm volatile(""); }
__device__ __noinline__ void c_exit_empty() { asm volatile(""); }
__device__ __noinline__ void c_entry(uint64_t *carry)
{
	carry[0] = gtimer();
	carry[1] = 1;
}
template <int MERGE>
__device__ __noinline__ void c_exit(uint64_t *carry, uint64_t *hist,
				    uint64_t *sink, unsigned never)
{
	if (carry[1] != 1)
		return;
	unsigned b = bucket(carry[0]);
	if (MERGE)
		red_add(smlane_word(hist, b, laneid()), 1);
	else if (b == never)
		*sink = b;
	carry[1] = 0;
}
__device__ __noinline__ uint64_t c_entry_ret() { return gtimer(); }
__device__ __noinline__ void c_exit_param(uint64_t t0, uint64_t *hist)
{
	red_add(smlane_word(hist, bucket(t0), laneid()), 1);
}

template <int MEM>
__device__ __forceinline__ uint64_t work(const uint4 *src)
{
	if (!MEM)
		return 0;
	size_t i = (size_t)blockIdx.x * blockDim.x + threadIdx.x;
	uint4 v = src[i & (SRC_N - 1)];
	return (uint64_t)v.x + v.y + v.z + v.w;
}

enum { P_NONE, P_CEMPTY, P_CCARRY, P_CSMLANE, P_CPARAM, P_RCARRY, P_RSMLANE, P_RWARP };

template <int P, int MEM>
__global__ void kern(uint64_t *hist, uint64_t *sink, const uint4 *src,
		     unsigned never)
{
	uint64_t carry[2];
	uint64_t t0 = 0;
	if (P == P_CEMPTY)
		c_entry_empty();
	else if (P == P_CCARRY || P == P_CSMLANE)
		c_entry(carry);
	else if (P == P_CPARAM)
		t0 = c_entry_ret();
	else if (P >= P_RCARRY)
		t0 = gtimer();
	uint64_t s = work<MEM>(src);
	if (s == never) // never true: the buffer holds 0x01 bytes
		*sink = s;
	if (P == P_CEMPTY) {
		c_exit_empty();
	} else if (P == P_CCARRY) {
		c_exit<0>(carry, hist, sink, never);
	} else if (P == P_CSMLANE) {
		c_exit<1>(carry, hist, sink, never);
	} else if (P == P_CPARAM) {
		c_exit_param(t0, hist);
	} else if (P == P_RCARRY) {
		if (bucket(t0) == never)
			*sink = 99;
	} else if (P == P_RSMLANE) {
		red_add(smlane_word(hist, bucket(t0), laneid()), 1);
	} else if (P == P_RWARP) {
		unsigned b = bucket(t0);
		unsigned mask = __activemask();
		unsigned peers = __match_any_sync(mask, b);
		unsigned leader = __ffs(peers) - 1;
		if (laneid() == leader)
			red_add(smlane_word(hist, b, leader), __popc(peers));
	}
}

typedef void (*kfn)(uint64_t *, uint64_t *, const uint4 *, unsigned);
struct K {
	const char *name;
	kfn f;
	bool merges;
};
#define KK(nm, p, merges)                                                      \
	{ nm "/empty", kern<p, 0>, merges }, { nm "/mem", kern<p, 1>, merges }
static K kernels[] = { KK("none", P_NONE, false),
		       KK("c_empty", P_CEMPTY, false),
		       KK("c_carry", P_CCARRY, false),
		       KK("c_smlane", P_CSMLANE, true),
		       KK("c_param", P_CPARAM, true),
		       KK("r_carry", P_RCARRY, false),
		       KK("r_smlane", P_RSMLANE, true),
		       KK("r_warp", P_RWARP, true) };
struct S {
	const char *name;
	unsigned blocks, tpb;
};
static S shapes[] = { { "anchor", 512, 512 },
		      { "large", 8192, 512 },
		      { "mmvq", 5632, 128 } };
struct Item {
	K *k;
	S *s;
	unsigned n;
};

#define HOLD_ITER_CAP (1u << 24)
__global__ void k_hold(volatile uint64_t *hostw, uint64_t cap_ns)
{
	if (threadIdx.x != 0)
		return;
	const uint64_t t0 = gtimer();
#pragma unroll 1
	for (uint32_t it = 0; it < HOLD_ITER_CAP; it++) {
		if (hostw[0] != 0)
			return;
		if (gtimer() - t0 > cap_ns)
			break;
		__nanosleep(256);
	}
	hostw[1] = hostw[1] + 1;
}

static uint64_t *hist, *sink;
static uint4 *src;
static volatile uint64_t *hold_host;
static uint64_t *hold_dev;
static cudaStream_t st;

static float time_batch(Item &it, unsigned n, cudaEvent_t e0, cudaEvent_t e1,
			bool *capped)
{
	uint64_t caps0 = hold_host[1];
	hold_host[0] = 0;
	__sync_synchronize();
	k_hold<<<1, 32, 0, st>>>(hold_dev, 200000000ull);
	CK(cudaEventRecord(e0, st));
	for (unsigned i = 0; i < n; i++)
		it.k->f<<<it.s->blocks, it.s->tpb, 0, st>>>(hist, sink, src, NEVER);
	CK(cudaEventRecord(e1, st));
	__sync_synchronize();
	hold_host[0] = 1;
	CK(cudaEventSynchronize(e1));
	float ms = 0;
	CK(cudaEventElapsedTime(&ms, e0, e1));
	*capped = hold_host[1] != caps0;
	return ms;
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: %s OUTDIR ROUNDS [seed]\n", argv[0]);
		return 1;
	}
	std::string out = argv[1];
	int rounds = atoi(argv[2]);
	uint64_t seed = argc > 3 ? strtoull(argv[3], nullptr, 0) : 20261117;
	CK(cudaSetDeviceFlags(cudaDeviceMapHost));
	CK(cudaStreamCreateWithFlags(&st, cudaStreamNonBlocking));
	CK(cudaMalloc(&hist, HIST_WORDS * 8));
	CK(cudaMalloc(&sink, 64));
	CK(cudaMalloc(&src, SRC_N * sizeof(uint4)));
	CK(cudaMemset(src, 1, SRC_N * sizeof(uint4)));
	uint64_t *hh;
	CK(cudaHostAlloc(&hh, 64, cudaHostAllocMapped));
	memset(hh, 0, 64);
	hold_host = hh;
	CK(cudaHostGetDevicePointer((void **)&hold_dev, hh, 0));
	int nsm = 0;
	CK(cudaDeviceGetAttribute(&nsm, cudaDevAttrMultiProcessorCount, 0));
	if (nsm > NSM_MAX)
		return 2;
	cudaEvent_t e0, e1;
	CK(cudaEventCreate(&e0));
	CK(cudaEventCreate(&e1));
	std::vector<Item> items;
	for (auto &s : shapes)
		for (auto &k : kernels)
			items.push_back({ &k, &s, 0 });
	FILE *fi = fopen((out + "/items.txt").c_str(), "w");
	std::vector<uint64_t> h(HIST_WORDS);
	int bad = 0;
	for (auto &it : items) {
		if (!it.k->merges)
			continue;
		CK(cudaMemsetAsync(hist, 0, HIST_WORDS * 8, st));
		it.k->f<<<it.s->blocks, it.s->tpb, 0, st>>>(hist, sink, src, NEVER);
		CK(cudaStreamSynchronize(st));
		CK(cudaGetLastError());
		CK(cudaMemcpy(h.data(), hist, HIST_WORDS * 8,
			      cudaMemcpyDeviceToHost));
		uint64_t sum = 0;
		for (auto v : h)
			sum += v;
		uint64_t want = (uint64_t)it.s->blocks * it.s->tpb;
		fprintf(fi, "validate %s %s sum=%" PRIu64 " want=%" PRIu64 " %s\n",
			it.k->name, it.s->name, sum, want,
			sum == want ? "ok" : "BAD");
		bad += sum != want;
	}
	if (bad) {
		fprintf(stderr, "validation failed\n");
		fclose(fi);
		return 3;
	}
	for (auto &it : items) {
		bool cp;
		unsigned n = 4;
		time_batch(it, n, e0, e1, &cp);
		for (;;) {
			float ms = time_batch(it, n, e0, e1, &cp);
			if (ms >= 2.0f || n >= (1u << 16)) {
				n = std::max(1u, (unsigned)(n * 4.0 / ms));
				break;
			}
			n *= 4;
		}
		it.n = std::min(n, 1u << 16);
		fprintf(fi, "item %s %s threads=%u n=%u\n", it.k->name,
			it.s->name, it.s->blocks * it.s->tpb, it.n);
	}
	fclose(fi);
	FILE *fr = fopen((out + "/raw.csv").c_str(), "w");
	fprintf(fr, "round,kernel,shape,threads,n,ms,capped\n");
	for (int r = 0; r < rounds; r++) {
		std::vector<size_t> order(items.size());
		for (size_t i = 0; i < order.size(); i++)
			order[i] = i;
		std::mt19937_64 rng(seed + (uint64_t)r);
		std::shuffle(order.begin(), order.end(), rng);
		for (size_t i : order) {
			auto &it = items[i];
			bool cp = false;
			time_batch(it, 1, e0, e1, &cp);
			float ms = time_batch(it, it.n, e0, e1, &cp);
			fprintf(fr, "%d,%s,%s,%u,%u,%.6f,%d\n", r, it.k->name,
				it.s->name, it.s->blocks * it.s->tpb, it.n, ms,
				(int)cp);
		}
		fflush(fr);
	}
	fclose(fr);
	fprintf(stderr, "done\n");
	return 0;
}

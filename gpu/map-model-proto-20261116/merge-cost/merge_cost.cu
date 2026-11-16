// merge_cost: what does a thread's exit merge into a shared histogram cost,
// when (nearly) every thread lands in the same bucket? Per thread execution,
// on top of an empty kernel, at step 0's shapes. Prototype step "price the
// exit merge before plumbing" (proto/map-model, 2026-11-16).
//
// Every timed kernel has the probes' structure: a kernel-scope carry (two u64
// in local memory, its generic address passed to __noinline__ entry and exit
// functions, as the routing passes it), the entry stores %globaltimer, the
// exit reads it back, takes bucket = 63 - clz(delta | 1) (data dependent, so
// ptxas cannot prove the address uniform and aggregate the atomics) and
// merges one count:
//   m_none     no probes (the delta reference)
//   m_carry    entry + exit, no merge (the bucket is consumed by a branch)
//   m_glob     red.add on hist[bucket]: one word per bucket for the grid
//   m_sm       hist[smid][bucket]: one word per bucket per SM
//   m_lane     hist[bucket][lane]: one word per bucket per lane
//   m_smlane   hist[bucket][smid][lane]: one word per bucket, SM and lane
// %smid and %laneid only spread the words: any value is correct, the drain
// sums every shard (an atomic add is correct whatever word it hits).
// Timing as step0/e2-cost: k_hold gate, N launches between two events,
// N calibrated to about 4 ms, ROUNDS rounds, items shuffled per round by
// mt19937_64(seed + round). Validation before timing: one launch of every
// merge kernel at every shape must add exactly `threads` counts in all.
// Output: OUT/raw.csv (round, item, n, ms), OUT/items.txt.
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

static constexpr int NB = 64;	   // buckets
static constexpr int NSM_MAX = 64;  // shards by %smid (%smid < 64 asserted)
static constexpr size_t HIST_WORDS = (size_t)NB * NSM_MAX * 32;

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

__device__ __noinline__ void entry_probe(uint64_t *carry)
{
	carry[0] = gtimer();
	carry[1] = 1; // valid mark
}
__device__ __forceinline__ unsigned bucket_of(uint64_t *carry)
{
	uint64_t d = gtimer() - carry[0];
	return 63 - __clzll(d | 1);
}
template <int V>
__device__ __noinline__ void exit_probe(uint64_t *carry, uint64_t *hist,
					uint64_t *sink)
{
	if (carry[1] != 1)
		return;
	unsigned b = bucket_of(carry);
	if (V == 0) {
		if (b == 99)
			*sink = b; // never: keeps the read and the bucket
	} else if (V == 1) {
		red_add(hist + b, 1);
	} else if (V == 2) {
		red_add(hist + (size_t)(smid() & (NSM_MAX - 1)) * NB + b, 1);
	} else if (V == 3) {
		red_add(hist + (size_t)b * 32 + laneid(), 1);
	} else if (V == 4) {
		red_add(hist + ((size_t)b * NSM_MAX + (smid() & (NSM_MAX - 1))) *
				       32 +
				laneid(),
			1);
	}
	carry[1] = 0;
}

extern "C" __global__ void m_none(uint64_t *hist, uint64_t *sink) {}

template <int V>
__device__ __forceinline__ void body(uint64_t *hist, uint64_t *sink)
{
	uint64_t carry[2];
	entry_probe(carry);
	exit_probe<V>(carry, hist, sink);
}
extern "C" __global__ void m_carry(uint64_t *h, uint64_t *s) { body<0>(h, s); }
extern "C" __global__ void m_glob(uint64_t *h, uint64_t *s) { body<1>(h, s); }
extern "C" __global__ void m_sm(uint64_t *h, uint64_t *s) { body<2>(h, s); }
extern "C" __global__ void m_lane(uint64_t *h, uint64_t *s) { body<3>(h, s); }
extern "C" __global__ void m_smlane(uint64_t *h, uint64_t *s) { body<4>(h, s); }

// the gate of step0/e2-cost (src/kernels.cu, k_hold)
#define HOLD_ITER_CAP (1u << 24)
extern "C" __global__ void k_hold(volatile uint64_t *hostw, uint64_t cap_ns)
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

typedef void (*kfn)(uint64_t *, uint64_t *);
struct K {
	const char *name;
	kfn f;
	bool merges;
};
static K kernels[] = { { "m_none", m_none, false },
		       { "m_carry", m_carry, false },
		       { "m_glob", m_glob, true },
		       { "m_sm", m_sm, true },
		       { "m_lane", m_lane, true },
		       { "m_smlane", m_smlane, true } };
struct S {
	const char *name;
	unsigned blocks, tpb;
};
static S shapes[] = { { "anchor", 512, 512 },
		      { "full", 24, 512 },
		      { "large", 8192, 512 } };
struct Item {
	K *k;
	S *s;
	unsigned n;
};

static uint64_t *hist, *sink;
static volatile uint64_t *hold_host;
static uint64_t *hold_dev;
static cudaStream_t st;

static float time_batch(Item &it, unsigned n, cudaEvent_t e0, cudaEvent_t e1,
			bool *capped)
{
	uint64_t caps0 = hold_host[1];
	hold_host[0] = 0;
	__sync_synchronize();
	uint64_t cap = 200000000ull;
	k_hold<<<1, 32, 0, st>>>(hold_dev, cap);
	CK(cudaEventRecord(e0, st));
	for (unsigned i = 0; i < n; i++)
		it.k->f<<<it.s->blocks, it.s->tpb, 0, st>>>(hist, sink);
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
	uint64_t seed = argc > 3 ? strtoull(argv[3], nullptr, 0) : 20261116;
	CK(cudaSetDeviceFlags(cudaDeviceMapHost));
	CK(cudaStreamCreateWithFlags(&st, cudaStreamNonBlocking));
	CK(cudaMalloc(&hist, HIST_WORDS * 8));
	CK(cudaMalloc(&sink, 64));
	uint64_t *hh;
	CK(cudaHostAlloc(&hh, 64, cudaHostAllocMapped));
	memset(hh, 0, 64);
	hold_host = hh;
	CK(cudaHostGetDevicePointer((void **)&hold_dev, hh, 0));
	int nsm = 0;
	CK(cudaDeviceGetAttribute(&nsm, cudaDevAttrMultiProcessorCount, 0));
	if (nsm > NSM_MAX) {
		fprintf(stderr, "more SMs than shards\n");
		return 2;
	}
	cudaEvent_t e0, e1;
	CK(cudaEventCreate(&e0));
	CK(cudaEventCreate(&e1));
	std::vector<Item> items;
	for (auto &s : shapes)
		for (auto &k : kernels)
			items.push_back({ &k, &s, 0 });
	FILE *fi = fopen((out + "/items.txt").c_str(), "w");
	// validation: every merge kernel adds exactly `threads` counts
	std::vector<uint64_t> h(HIST_WORDS);
	int bad = 0;
	for (auto &it : items) {
		if (!it.k->merges)
			continue;
		CK(cudaMemsetAsync(hist, 0, HIST_WORDS * 8, st));
		it.k->f<<<it.s->blocks, it.s->tpb, 0, st>>>(hist, sink);
		CK(cudaStreamSynchronize(st));
		CK(cudaGetLastError());
		CK(cudaMemcpy(h.data(), hist, HIST_WORDS * 8,
			      cudaMemcpyDeviceToHost));
		uint64_t sum = 0;
		int nz = 0;
		for (auto v : h) {
			sum += v;
			nz += v != 0;
		}
		uint64_t want = (uint64_t)it.s->blocks * it.s->tpb;
		fprintf(fi, "validate %s %s sum=%" PRIu64 " want=%" PRIu64
			    " nonzero_words=%d %s\n",
			it.k->name, it.s->name, sum, want, nz,
			sum == want ? "ok" : "BAD");
		bad += sum != want;
	}
	if (bad) {
		fprintf(stderr, "validation failed\n");
		fclose(fi);
		return 3;
	}
	// calibration: about 4 ms per batch
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
			time_batch(it, 1, e0, e1, &cp); // untimed
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

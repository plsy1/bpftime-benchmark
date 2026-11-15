// pubc: step 0b, check C4 (batched publication), the correctness harness.
// Derived from step 0's pub4 test 1 (e4-publish/src/pub4.cu): the same
// registered memory (bpftime's way, a second unregistered mapping for the CPU
// readers), the same payload values, reverse reads, poison, release-store
// acks, re-reads of violating words, launch chunking.
//
// 384 warps (48 blocks x 256 threads, all resident at full occupancy). Warp w
// belongs to chunk c and has position k in it:
//   interleaved (default): c = w % NC, k = w / NC (NC = 384 / G chunks), so a
//                          chunk's warps come from different blocks and SMs
//   --contiguous:          c = w / G, k = w % G
// Every chunk has S = 4 slots; round r of a chunk uses slot (r-1) % S. Round r
// of warp w:
//   lane 0 waits (capped) until ack[c][s] == r - S, polling with
//     ld.relaxed.sys; then the record-writing lanes ld.acquire.sys the ack
//     (an acquire pattern: the CPU's poison stores of round r - S happen
//     before this round's record stores)
//   the record: w32 lane 0 writes 4 x u64 at rec[c][s] + 4 k;
//               l8  every lane writes 1 x u64 at rec[c][s] + 32 k + lane
//   __syncwarp (l8)
//   variant's contributor release, then the chunk-slot counter increment
//     (device memory; cumulative, so the u-th use of a slot completes at
//     u * G increments: no reset, no modulo)
//   the completer (the warp, or lane for ar32, whose increment returns
//     u * G - 1): variant's acquire and system fence, then
//     st.relaxed.sys flag[c][s] = r << 20 | w
// CPU readers (threads split the chunks): acquire-load the flag (LDAR); when
// it shows round r, read every record word of the chunk in reverse order
// (last warp, last word first), check it, overwrite every word with poison,
// then release-store (STLR) ack = r.
//
// Variants (contributor | completer):
//   ar_sc    fence.acq_rel.gpu; atom.relaxed.gpu | fence.acq_rel.gpu; fence.sc.sys
//   tf_sc    fence.sc.gpu;      atom.relaxed.gpu | fence.acq_rel.gpu; fence.sc.sys
//   rel_sc   atom.release.gpu                    | fence.acq_rel.gpu; fence.sc.sys
//   ar_ar    fence.acq_rel.gpu; atom.relaxed.gpu | fence.acq_rel.sys
//   rel_ar   atom.release.gpu                    | fence.acq_rel.sys
//   ar32_sc  (l8) every lane: fence.acq_rel.gpu; atom.relaxed.gpu (32 G per
//            use) | fence.acq_rel.gpu; fence.sc.sys
//   ar32_ar  (l8) as ar32_sc | fence.acq_rel.sys
//   ar_aa    fence.acq_rel.gpu; atom.relaxed.gpu | fence.acq_rel.gpu; fence.acq_rel.sys
//   rmw_ar   atom.acq_rel.gpu (release and, for the completer, acquire) | fence.acq_rel.sys
//   ar32_aa  (l8) as ar32_sc | fence.acq_rel.gpu; fence.acq_rel.sys
//   controls:
//   none     atom.relaxed.gpu                    | (nothing)          no fence at all
//   sysonly  atom.relaxed.gpu                    | fence.acq_rel.gpu; fence.sc.sys
//   nosys    fence.acq_rel.gpu; atom.relaxed.gpu | fence.acq_rel.gpu  no system fence
//   early    the increment and the completer's flag store come BEFORE the
//            record stores, no fence                                  must fail
// --time-fence: the completer brackets its system fence with %globaltimer
// reads (histograms: first round of a launch, other rounds).
//
// Every device wait is capped (iteration cap and globaltimer deadline); cap
// and deadline hits are counted and stop the warp.
#include "../common/step0b_regmem.h"

#include <cuda_runtime.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdarg>
#include <string>
#include <thread>
#include <vector>

#define CUDA_OK(call)                                                          \
	do {                                                                   \
		cudaError_t e_ = (call);                                       \
		if (e_ != cudaSuccess) {                                       \
			fprintf(stderr, "%s:%d: %s -> %s\n", __FILE__,         \
				__LINE__, #call, cudaGetErrorString(e_));      \
			std::exit(11);                                         \
		}                                                              \
	} while (0)

#define FULL_MASK 0xffffffffu
static constexpr int LINE = 128;
static constexpr uint64_t LINE_WORDS = LINE / 8;
static constexpr uint32_t WAIT_CAP = 1u << 26; // ack waits (with nanosleep)
static constexpr uint32_t CONFIRM_CAP = 1u << 16; // acquire re-reads of a seen ack
static constexpr uint32_t FH_BINS = 4096;
static constexpr uint64_t FH_NS = 64; // up to 262 us
static constexpr int FLAG_SHIFT = 20; // flag = r << 20 | completer warp

enum Variant {
	V_AR_SC = 0,
	V_TF_SC,
	V_REL_SC,
	V_AR_AR,
	V_REL_AR,
	V_AR32_SC,
	V_AR32_AR,
	V_AR_AA,
	V_RMW_AR,
	V_AR32_AA,
	V_NONE,
	V_SYSONLY,
	V_NOSYS,
	V_EARLY,
	NVARIANTS
};
static const char *VNAMES[] = { "ar_sc",   "tf_sc",   "rel_sc",  "ar_ar",
				"rel_ar",  "ar32_sc", "ar32_ar", "ar_aa",
				"rmw_ar",  "ar32_aa", "none",    "sysonly",
				"nosys",   "early" };
enum Layout { L_W32 = 0, L_L8 = 1 };

// ---------------------------------------------------------------- device
__device__ __forceinline__ uint64_t gtimer()
{
	uint64_t t;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	return t;
}
__device__ __forceinline__ void st_u64(uint64_t *p, uint64_t v)
{
	asm volatile("st.global.u64 [%0], %1;" ::"l"(p), "l"(v) : "memory");
}
__device__ __forceinline__ void st_relaxed_sys(uint64_t *p, uint64_t v)
{
	asm volatile("st.relaxed.sys.global.u64 [%0], %1;" ::"l"(p), "l"(v)
		     : "memory");
}
__device__ __forceinline__ uint64_t ld_relaxed_sys(const uint64_t *p)
{
	uint64_t v;
	asm volatile("ld.relaxed.sys.global.u64 %0, [%1];"
		     : "=l"(v)
		     : "l"(p)
		     : "memory");
	return v;
}
__device__ __forceinline__ uint64_t ld_acquire_sys(const uint64_t *p)
{
	uint64_t v;
	asm volatile("ld.acquire.sys.global.u64 %0, [%1];"
		     : "=l"(v)
		     : "l"(p)
		     : "memory");
	return v;
}
__device__ __forceinline__ void fence_sc_gpu()
{
	asm volatile("fence.sc.gpu;" ::: "memory");
}
__device__ __forceinline__ void fence_acq_rel_gpu()
{
	asm volatile("fence.acq_rel.gpu;" ::: "memory");
}
__device__ __forceinline__ void fence_sc_sys()
{
	asm volatile("fence.sc.sys;" ::: "memory");
}
__device__ __forceinline__ void fence_acq_rel_sys()
{
	asm volatile("fence.acq_rel.sys;" ::: "memory");
}
__device__ __forceinline__ uint64_t atom_relaxed_gpu_add(uint64_t *p, uint64_t v)
{
	uint64_t r;
	asm volatile("atom.relaxed.gpu.global.add.u64 %0, [%1], %2;"
		     : "=l"(r)
		     : "l"(p), "l"(v)
		     : "memory");
	return r;
}
__device__ __forceinline__ uint64_t atom_acq_rel_gpu_add(uint64_t *p, uint64_t v)
{
	uint64_t r;
	asm volatile("atom.acq_rel.gpu.global.add.u64 %0, [%1], %2;"
		     : "=l"(r)
		     : "l"(p), "l"(v)
		     : "memory");
	return r;
}
__device__ __forceinline__ uint64_t atom_release_gpu_add(uint64_t *p, uint64_t v)
{
	uint64_t r;
	asm volatile("atom.release.gpu.global.add.u64 %0, [%1], %2;"
		     : "=l"(r)
		     : "l"(p), "l"(v)
		     : "memory");
	return r;
}

// step 0's pub4 payload value
__host__ __device__ __forceinline__ uint64_t t1_value(uint64_t r, uint64_t w,
						      uint64_t lane, uint64_t j)
{
	return (r << 32) | (w << 16) | (lane << 8) | j;
}

struct Geom {
	uint32_t nwarps, G, nchunks, S;
	uint32_t contiguous, rec_words; // 4 (w32) or 32 (l8)
	uint32_t time_fence, pad;
	uint64_t *flags, *acks, *rec; // registered (device pointers)
	uint64_t *cnt; // device: per (chunk, slot)
	uint64_t *status; // device: per warp
	unsigned long long *fhist; // device: 2 x FH_BINS
};
static constexpr int STATUS_WORDS = 8;
// status per warp: [0] last round published, [1] dead, [2] cap hits,
// [3] deadline hits, [4] completions, [5] sum of fence ns, [6] max fence ns

template <int L, int V>
__device__ __forceinline__ void pc_body(const Geom &g, uint32_t r_begin,
					uint32_t r_end, uint64_t budget_ns)
{
	const uint32_t w = (blockIdx.x * blockDim.x + threadIdx.x) >> 5;
	const uint32_t lane = threadIdx.x & 31;
	if (w >= g.nwarps)
		return;
	uint64_t *st = g.status + (uint64_t)w * STATUS_WORDS;
	if (*(volatile uint64_t *)(st + 1) != 0)
		return;
	const uint32_t c = g.contiguous ? w / g.G : w % g.nchunks;
	const uint32_t k = g.contiguous ? w % g.G : w / g.nchunks;
	const uint64_t deadline = gtimer() + budget_ns;
	constexpr bool AR32 = V == V_AR32_SC || V == V_AR32_AR || V == V_AR32_AA;
	const uint64_t per_use = (AR32 ? 32ull : 1ull) * g.G;
#pragma unroll 1
	for (uint32_t r = r_begin; r < r_end; r++) {
		const uint32_t s = (r - 1) % g.S;
		const uint64_t cs = (uint64_t)c * g.S + s;
		uint64_t *flag = g.flags + cs * LINE_WORDS;
		uint64_t *ack = g.acks + cs * LINE_WORDS;
		uint64_t *rec = g.rec + (cs * g.G + k) * g.rec_words;
		uint64_t *ctr = g.cnt + cs * 4;
		const uint64_t want = r > g.S ? r - g.S : 0;
		const uint64_t target = ((uint64_t)(r - 1) / g.S + 1) * per_use;
		int ok = 1;
		if (lane == 0) {
			uint32_t it = 0;
			int late = 0;
#pragma unroll 1
			for (; it < WAIT_CAP; it++) {
				if (ld_relaxed_sys(ack) == want)
					break;
				if ((it & 255) == 255 && gtimer() > deadline) {
					late = 1;
					break;
				}
				__nanosleep(64);
			}
			if (late || it == WAIT_CAP) {
				ok = 0;
				st[1] = 1;
				st[2] += (it == WAIT_CAP);
				st[3] += late;
			}
		}
		ok = __shfl_sync(FULL_MASK, ok, 0);
		if (!ok)
			return; // this warp stops; its chunk will not complete
		// Every record-writing lane acquires the CPU's ack itself (an
		// acquire operation that reads the CPU's release store, so the
		// CPU's poison stores of round r - S precede this round's record
		// stores in causality order). Lane 0 has seen the value already;
		// a lane that does not see it within the cap stops the warp.
		{
			int seen = 1;
			if (L == L_L8 || lane == 0) {
				uint32_t it = 0;
#pragma unroll 1
				for (; it < CONFIRM_CAP; it++)
					if (ld_acquire_sys(ack) == want)
						break;
				seen = it < CONFIRM_CAP;
			}
			if (!__all_sync(FULL_MASK, seen)) {
				if (lane == 0) {
					st[1] = 1;
					st[2]++;
				}
				return;
			}
		}
		if (V == V_EARLY) { // must fail: count and flag before the record
			if (lane == 0) {
				const uint64_t old = atom_relaxed_gpu_add(ctr, 1);
				if (old + 1 == target) {
					st_relaxed_sys(flag, ((uint64_t)r << FLAG_SHIFT) | w);
					st[4]++;
				}
			}
			__syncwarp(FULL_MASK);
		}
		if (L == L_W32) {
			if (lane == 0) {
				st_u64(rec + 0, t1_value(r, w, 0, 0));
				st_u64(rec + 1, t1_value(r, w, 0, 1));
				st_u64(rec + 2, t1_value(r, w, 0, 2));
				st_u64(rec + 3, t1_value(r, w, 0, 3));
			}
		} else {
			st_u64(rec + lane, t1_value(r, w, lane, 0));
		}
		if (V == V_EARLY) {
			if (lane == 0)
				st[0] = r;
			continue;
		}
		uint64_t old = 0;
		bool contributor = lane == 0;
		if (AR32) {
			contributor = true;
			fence_acq_rel_gpu();
			old = atom_relaxed_gpu_add(ctr, 1);
		} else {
			__syncwarp(FULL_MASK);
			if (lane == 0) {
				if (V == V_NONE || V == V_SYSONLY) {
					old = atom_relaxed_gpu_add(ctr, 1);
				} else if (V == V_TF_SC) {
					fence_sc_gpu();
					old = atom_relaxed_gpu_add(ctr, 1);
				} else if (V == V_AR_SC || V == V_AR_AR || V == V_AR_AA ||
					   V == V_NOSYS) {
					fence_acq_rel_gpu();
					old = atom_relaxed_gpu_add(ctr, 1);
				} else if (V == V_RMW_AR) {
					old = atom_acq_rel_gpu_add(ctr, 1);
				} else { // V_REL_SC, V_REL_AR
					old = atom_release_gpu_add(ctr, 1);
				}
			}
		}
		if (contributor && old + 1 == target) {
			uint64_t t0 = 0, t1 = 0;
			if (V == V_NONE) {
			} else if (V == V_NOSYS) {
				fence_acq_rel_gpu();
			} else if (V == V_AR_AR || V == V_REL_AR || V == V_AR32_AR ||
				   V == V_RMW_AR || V == V_AR_AA || V == V_AR32_AA) {
				if (V == V_AR_AA || V == V_AR32_AA)
					fence_acq_rel_gpu(); // closes the acquire pattern
				if (g.time_fence)
					t0 = gtimer();
				fence_acq_rel_sys();
				if (g.time_fence)
					t1 = gtimer();
			} else {
				fence_acq_rel_gpu();
				if (g.time_fence)
					t0 = gtimer();
				fence_sc_sys();
				if (g.time_fence)
					t1 = gtimer();
			}
			st_relaxed_sys(flag, ((uint64_t)r << FLAG_SHIFT) | w);
			atomicAdd((unsigned long long *)st + 4, 1ull);
			if (g.time_fence) {
				const uint64_t d = t1 - t0;
				const uint64_t b = d / FH_NS < FH_BINS - 1 ?
							   d / FH_NS :
							   FH_BINS - 1;
				atomicAdd(g.fhist + (r == r_begin ? 0 : FH_BINS) + b,
					  1ull);
				atomicAdd((unsigned long long *)st + 5,
					  (unsigned long long)d);
				atomicMax((unsigned long long *)st + 6,
					  (unsigned long long)d);
			}
		}
		if (lane == 0)
			st[0] = r;
	}
}

#define PC(L, LN, V, VN)                                                       \
	extern "C" __global__ void pc_##LN##_##VN(Geom g, uint32_t rb,         \
						  uint32_t re, uint64_t bn)    \
	{                                                                      \
		pc_body<L, V>(g, rb, re, bn);                                \
	}
#define PC_BOTH(V, VN)                                                         \
	PC(L_W32, w32, V, VN)                                                  \
	PC(L_L8, l8, V, VN)
PC_BOTH(V_AR_SC, ar_sc)
PC_BOTH(V_TF_SC, tf_sc)
PC_BOTH(V_REL_SC, rel_sc)
PC_BOTH(V_AR_AR, ar_ar)
PC_BOTH(V_REL_AR, rel_ar)
PC(L_L8, l8, V_AR32_SC, ar32_sc)
PC(L_L8, l8, V_AR32_AR, ar32_ar)
PC_BOTH(V_AR_AA, ar_aa)
PC_BOTH(V_RMW_AR, rmw_ar)
PC(L_L8, l8, V_AR32_AA, ar32_aa)
PC_BOTH(V_NONE, none)
PC_BOTH(V_SYSONLY, sysonly)
PC_BOTH(V_NOSYS, nosys)
PC_BOTH(V_EARLY, early)

typedef void (*KernFn)(Geom, uint32_t, uint32_t, uint64_t);
static KernFn kernel_for(int L, int V)
{
	if (L == L_W32) {
		switch (V) {
		case V_AR_SC: return pc_w32_ar_sc;
		case V_TF_SC: return pc_w32_tf_sc;
		case V_REL_SC: return pc_w32_rel_sc;
		case V_AR_AR: return pc_w32_ar_ar;
		case V_REL_AR: return pc_w32_rel_ar;
		case V_AR_AA: return pc_w32_ar_aa;
		case V_RMW_AR: return pc_w32_rmw_ar;
		case V_NONE: return pc_w32_none;
		case V_SYSONLY: return pc_w32_sysonly;
		case V_NOSYS: return pc_w32_nosys;
		case V_EARLY: return pc_w32_early;
		}
	} else {
		switch (V) {
		case V_AR_SC: return pc_l8_ar_sc;
		case V_TF_SC: return pc_l8_tf_sc;
		case V_REL_SC: return pc_l8_rel_sc;
		case V_AR_AR: return pc_l8_ar_ar;
		case V_REL_AR: return pc_l8_rel_ar;
		case V_AR32_SC: return pc_l8_ar32_sc;
		case V_AR32_AR: return pc_l8_ar32_ar;
		case V_AR_AA: return pc_l8_ar_aa;
		case V_RMW_AR: return pc_l8_rmw_ar;
		case V_AR32_AA: return pc_l8_ar32_aa;
		case V_NONE: return pc_l8_none;
		case V_SYSONLY: return pc_l8_sysonly;
		case V_NOSYS: return pc_l8_nosys;
		case V_EARLY: return pc_l8_early;
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------- host
static double now_s()
{
	return std::chrono::duration<double>(
		       std::chrono::steady_clock::now().time_since_epoch())
		.count();
}
static inline void cpu_relax()
{
	asm volatile("yield" ::: "memory");
}
static std::string fmt(const char *f, ...)
{
	va_list ap, ap2;
	va_start(ap, f);
	va_copy(ap2, ap);
	const int n = vsnprintf(nullptr, 0, f, ap);
	va_end(ap);
	std::string s(n > 0 ? n : 0, '\0');
	vsnprintf(s.data(), s.size() + 1, f, ap2);
	va_end(ap2);
	return s;
}

struct Opts {
	std::string variant = "ar_sc", layout = "w32";
	uint32_t G = 48;
	uint64_t records = 10000000; // warp records requested (all chunks)
	double budget_s = 30;
	uint64_t chunk_rounds = 4; // rounds per launch
	bool contiguous = false, time_fence = false, forward = false,
	     no_poison = false;
	int S = 4, readers = 4, blocks = 48, tpb = 256;
	int log_max = 20, reread_max = 50;
	std::string out;
};

struct ReaderStats {
	uint64_t chunk_rounds = 0, records = 0, viol_rounds = 0, bad_words = 0,
		 stale = 0, poison = 0, future = 0, other = 0, healed = 0,
		 persistent = 0, rereads = 0, anomalies = 0,
		 viol_first_round = 0, bad_words_completer = 0,
		 bad_words_others = 0;
	int heal_ms_max = 0;
	std::vector<std::string> log;
};

struct HostGeom {
	uint32_t nwarps, G, nchunks, S, rec_words;
	bool contiguous;
	uint8_t *view; // second mapping
	size_t flags_off, acks_off, rec_off;
	uint32_t warp_of(uint32_t c, uint32_t k) const
	{
		return contiguous ? c * G + k : k * nchunks + c;
	}
};

static void reader(int id, const Opts &o, const HostGeom &h,
		   uint64_t rounds_per_chunk, std::atomic<bool> *stop,
		   ReaderStats *rs)
{
	std::vector<uint32_t> mine;
	for (uint32_t c = id; c < h.nchunks; c += o.readers)
		mine.push_back(c);
	std::vector<uint64_t> exp(mine.size(), 1);
	const uint32_t nw = h.G * h.rec_words;
	std::vector<uint64_t> seen(nw);
	std::vector<uint32_t> bad;
	uint64_t *flags = (uint64_t *)(h.view + h.flags_off);
	uint64_t *acks = (uint64_t *)(h.view + h.acks_off);
	uint64_t *recs = (uint64_t *)(h.view + h.rec_off);
	auto want_of = [&](uint32_t c, uint64_t r, uint32_t q) {
		const uint32_t k = q / h.rec_words, x = q % h.rec_words;
		const uint32_t w = h.warp_of(c, k);
		return h.rec_words == 4 ? t1_value(r, w, 0, x) :
					  t1_value(r, w, x, 0);
	};
	uint64_t idle = 0;
	while (!stop->load(std::memory_order_relaxed)) {
		bool progress = false;
		for (size_t i = 0; i < mine.size(); i++) {
			const uint64_t r = exp[i];
			if (r > rounds_per_chunk)
				continue;
			const uint32_t c = mine[i];
			const uint64_t cs = (uint64_t)c * h.S + (r - 1) % h.S;
			uint64_t *flag = flags + cs * LINE_WORDS;
			uint64_t *ack = acks + cs * LINE_WORDS;
			volatile uint64_t *rec = recs + cs * nw;
			const uint64_t f = __atomic_load_n(flag, __ATOMIC_ACQUIRE);
			if ((f >> FLAG_SHIFT) != r) {
				const uint64_t prev = r > h.S ? r - h.S : 0;
				if ((f >> FLAG_SHIFT) != prev) {
					rs->anomalies++;
					if ((int)rs->log.size() < o.log_max)
						rs->log.push_back(fmt(
							"anomaly chunk %u round %" PRIu64
							" flag %" PRIx64,
							c, r, f));
				}
				continue;
			}
			const uint32_t completer = (uint32_t)(f & ((1u << FLAG_SHIFT) - 1));
			for (uint32_t n = 0; n < nw; n++) {
				const uint32_t q = o.forward ? n : nw - 1 - n;
				seen[q] = rec[q];
			}
			bad.clear();
			for (uint32_t q = 0; q < nw; q++)
				if (seen[q] != want_of(c, r, q))
					bad.push_back(q);
			if (!bad.empty()) {
				rs->viol_rounds++;
				if (o.chunk_rounds && (r - 1) % o.chunk_rounds == 0)
					rs->viol_first_round++;
				rs->bad_words += bad.size();
				for (uint32_t q : bad) {
					const uint32_t w = h.warp_of(c, q / h.rec_words);
					if (w == completer)
						rs->bad_words_completer++;
					else
						rs->bad_words_others++;
					const uint64_t sr = seen[q] >> 32;
					if ((seen[q] >> 48) == 0xdead)
						rs->poison++;
					else if (sr < r)
						rs->stale++;
					else if (sr > r)
						rs->future++;
					else
						rs->other++;
				}
				if ((int)rs->log.size() < o.log_max) {
					const uint32_t q = bad[0];
					rs->log.push_back(fmt(
						"violation chunk %u round %" PRIu64
						": %zu bad words, first word %u (warp %u, completer %u) seen %016" PRIx64
						" want %016" PRIx64,
						c, r, bad.size(), q,
						h.warp_of(c, q / h.rec_words), completer,
						seen[q], want_of(c, r, q)));
				}
				// re-read after 1, 2, 4, ... 512 ms: healed = the
				// words arrived late (an ordering violation);
				// never healed = a harness bug
				if (rs->rereads < (uint64_t)o.reread_max) {
					rs->rereads++;
					bool healed = false;
					int waited = 0;
					for (int ms = 1; ms <= 512 && !healed; ms *= 2) {
						std::this_thread::sleep_for(
							std::chrono::milliseconds(ms));
						waited += ms;
						healed = true;
						for (uint32_t q : bad)
							if (rec[q] != want_of(c, r, q))
								healed = false;
					}
					if (healed) {
						rs->healed++;
						rs->heal_ms_max =
							std::max(rs->heal_ms_max, waited);
					} else {
						rs->persistent++;
					}
				}
			}
			if (!o.no_poison)
				for (uint32_t q = 0; q < nw; q++)
					rec[q] = 0xdeadull << 48 | q;
			__atomic_store_n(ack, r, __ATOMIC_RELEASE);
			exp[i] = r + 1;
			// read by the main thread while this reader runs
			__atomic_store_n(&rs->chunk_rounds, rs->chunk_rounds + 1,
					 __ATOMIC_RELAXED);
			rs->records += h.G;
			progress = true;
		}
		if (!progress) {
			if (++idle > 64) {
				std::this_thread::yield();
				idle = 0;
			} else {
				cpu_relax();
			}
		}
	}
}

static void usage()
{
	fprintf(stderr,
		"pubc --variant ar_sc|tf_sc|rel_sc|ar_ar|rel_ar|ar32_sc|ar32_ar|ar_aa|rmw_ar|ar32_aa|none|sysonly|nosys|early\n"
		"     --layout w32|l8 --G warps-per-chunk (divides 384) --records N (warp records)\n"
		"     --budget-s S --chunk-rounds n --contiguous --time-fence --forward --no-poison\n"
		"     --readers n --out file.json\n");
}

int main(int argc, char **argv)
{
	Opts o;
	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		auto val = [&]() -> std::string {
			if (i + 1 >= argc) {
				usage();
				exit(2);
			}
			return argv[++i];
		};
		if (a == "--variant")
			o.variant = val();
		else if (a == "--layout")
			o.layout = val();
		else if (a == "--G")
			o.G = (uint32_t)atoi(val().c_str());
		else if (a == "--records")
			o.records = strtoull(val().c_str(), nullptr, 0);
		else if (a == "--budget-s")
			o.budget_s = atof(val().c_str());
		else if (a == "--chunk-rounds")
			o.chunk_rounds = strtoull(val().c_str(), nullptr, 0);
		else if (a == "--contiguous")
			o.contiguous = true;
		else if (a == "--time-fence")
			o.time_fence = true;
		else if (a == "--forward")
			o.forward = true;
		else if (a == "--no-poison")
			o.no_poison = true;
		else if (a == "--readers")
			o.readers = atoi(val().c_str());
		else if (a == "--out")
			o.out = val();
		else {
			usage();
			return 2;
		}
	}
	int v = -1;
	for (int k = 0; k < NVARIANTS; k++)
		if (o.variant == VNAMES[k])
			v = k;
	const int L = o.layout == "w32" ? L_W32 : o.layout == "l8" ? L_L8 : -1;
	const uint32_t nwarps = (uint32_t)(o.blocks * o.tpb / 32);
	if (v < 0 || L < 0 || o.G == 0 || nwarps % o.G != 0 ||
	    ((v == V_AR32_SC || v == V_AR32_AR || v == V_AR32_AA) && L != L_L8) ||
	    o.chunk_rounds == 0) {
		usage();
		return 2;
	}
	KernFn kern = kernel_for(L, v);
	CUDA_OK(cudaSetDeviceFlags(cudaDeviceScheduleBlockingSync));
	CUDA_OK(cudaFree(0));
	step0b_primary_ctx(0);
	{ // every warp must be resident: a chunk waits for all its warps
		int occ = 0, sms = 0;
		CUDA_OK(cudaOccupancyMaxActiveBlocksPerMultiprocessor(
			&occ, (const void *)kern, o.tpb, 0));
		CUDA_OK(cudaDeviceGetAttribute(&sms, cudaDevAttrMultiProcessorCount, 0));
		if ((int64_t)occ * sms < o.blocks) {
			fprintf(stderr, "not all blocks resident: %d x %d < %d\n",
				occ, sms, o.blocks);
			return 4;
		}
	}
	HostGeom h;
	h.nwarps = nwarps;
	h.G = o.G;
	h.nchunks = nwarps / o.G;
	h.S = (uint32_t)o.S;
	h.rec_words = L == L_W32 ? 4 : 32;
	h.contiguous = o.contiguous;
	const uint64_t ncs = (uint64_t)h.nchunks * h.S;
	h.flags_off = 0;
	h.acks_off = ncs * LINE;
	h.rec_off = 2 * ncs * LINE;
	uint64_t seg = h.rec_off + ncs * h.G * h.rec_words * 8;
	seg = ((seg + (1 << 20) - 1) >> 20) << 20;
	step0b_regmem m;
	const std::string shm = fmt("step0b_c4p_%d", (int)getpid());
	if (!step0b_regmem_create(m, shm, seg, true))
		return 3;
	memset(m.host2, 0, seg);
	h.view = (uint8_t *)m.host2;

	Geom g{};
	g.nwarps = nwarps;
	g.G = o.G;
	g.nchunks = h.nchunks;
	g.S = h.S;
	g.contiguous = o.contiguous;
	g.rec_words = h.rec_words;
	g.time_fence = o.time_fence;
	g.flags = (uint64_t *)(m.dev + h.flags_off);
	g.acks = (uint64_t *)(m.dev + h.acks_off);
	g.rec = (uint64_t *)(m.dev + h.rec_off);
	const size_t status_bytes = (size_t)nwarps * STATUS_WORDS * 8;
	CUDA_OK(cudaMalloc(&g.cnt, ncs * 4 * 8));
	CUDA_OK(cudaMemset(g.cnt, 0, ncs * 4 * 8));
	CUDA_OK(cudaMalloc(&g.status, status_bytes));
	CUDA_OK(cudaMemset(g.status, 0, status_bytes));
	CUDA_OK(cudaMalloc(&g.fhist, 2 * FH_BINS * 8));
	CUDA_OK(cudaMemset(g.fhist, 0, 2 * FH_BINS * 8));

	const uint64_t rpc = (o.records + nwarps - 1) / nwarps; // rounds per chunk
	std::atomic<bool> stop{ false };
	const int nreaders = std::min<int>(o.readers, (int)h.nchunks);
	o.readers = nreaders;
	std::vector<ReaderStats> rs(nreaders);
	std::vector<std::thread> th;
	for (int i = 0; i < nreaders; i++)
		th.emplace_back(reader, i, std::cref(o), std::cref(h), rpc, &stop,
				&rs[i]);
	uint64_t rb = 1;
	int launches = 0;
	bool budget_hit = false;
	const double t_gpu0 = now_s();
	while (rb <= rpc) {
		// the budget counts GPU-loop time, not CUDA and segment setup
		if (now_s() - t_gpu0 > o.budget_s) {
			budget_hit = true;
			break;
		}
		const uint64_t re = std::min<uint64_t>(rb + o.chunk_rounds, rpc + 1);
		kern<<<o.blocks, o.tpb>>>(g, (uint32_t)rb, (uint32_t)re,
					  5000000000ull);
		CUDA_OK(cudaGetLastError());
		CUDA_OK(cudaDeviceSynchronize());
		launches++;
		rb = re;
		std::vector<uint64_t> sv(nwarps * STATUS_WORDS);
		CUDA_OK(cudaMemcpy(sv.data(), g.status, status_bytes,
				   cudaMemcpyDeviceToHost));
		bool any_dead = false;
		for (uint32_t w = 0; w < nwarps; w++)
			any_dead |= sv[w * STATUS_WORDS + 1] != 0;
		if (any_dead) // a dead warp stalls its chunk for good
			break;
	}
	const double t_gpu = now_s() - t_gpu0;
	std::vector<uint64_t> sv(nwarps * STATUS_WORDS);
	CUDA_OK(cudaMemcpy(sv.data(), g.status, status_bytes,
			   cudaMemcpyDeviceToHost));
	uint64_t warp_rounds = 0, dead = 0, caps = 0, lates = 0, completions = 0,
		 fsum = 0, fmax = 0;
	for (uint32_t w = 0; w < nwarps; w++) {
		const uint64_t *x = &sv[(size_t)w * STATUS_WORDS];
		warp_rounds += x[0];
		dead += x[1] != 0;
		caps += x[2];
		lates += x[3];
		completions += x[4];
		fsum += x[5];
		fmax = std::max(fmax, x[6]);
	}
	// chunk-rounds published: every chunk completes the rounds all its warps
	// did; the readers verify them
	auto verified = [&]() {
		uint64_t s = 0;
		for (auto &x : rs)
			s += __atomic_load_n(&x.chunk_rounds, __ATOMIC_RELAXED);
		return s;
	};
	const double t_wait0 = now_s();
	while (verified() < completions && now_s() - t_wait0 < 5)
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	stop = true;
	for (auto &t : th)
		t.join();
	ReaderStats tot;
	for (auto &x : rs) {
		tot.chunk_rounds += x.chunk_rounds;
		tot.records += x.records;
		tot.viol_rounds += x.viol_rounds;
		tot.bad_words += x.bad_words;
		tot.stale += x.stale;
		tot.poison += x.poison;
		tot.future += x.future;
		tot.other += x.other;
		tot.healed += x.healed;
		tot.persistent += x.persistent;
		tot.rereads += x.rereads;
		tot.anomalies += x.anomalies;
		tot.viol_first_round += x.viol_first_round;
		tot.bad_words_completer += x.bad_words_completer;
		tot.bad_words_others += x.bad_words_others;
		tot.heal_ms_max = std::max(tot.heal_ms_max, x.heal_ms_max);
		for (auto &l : x.log)
			if ((int)tot.log.size() < o.log_max)
				tot.log.push_back(l);
	}
	std::string fence_json = "null";
	if (o.time_fence) {
		std::vector<unsigned long long> hh(2 * FH_BINS);
		CUDA_OK(cudaMemcpy(hh.data(), g.fhist, 2 * FH_BINS * 8,
				   cudaMemcpyDeviceToHost));
		auto pct = [&](int part) {
			const unsigned long long *x = hh.data() + part * FH_BINS;
			unsigned long long n = 0;
			for (uint32_t b = 0; b < FH_BINS; b++)
				n += x[b];
			auto at = [&](double q) -> long long {
				if (!n)
					return -1;
				unsigned long long acc = 0;
				for (uint32_t b = 0; b < FH_BINS; b++) {
					acc += x[b];
					if (acc >= q * n)
						return b == FH_BINS - 1 ?
							       -2 :
							       (long long)((b + 1) * FH_NS);
				}
				return -2;
			};
			return fmt("{\"n\": %llu, \"p50\": %lld, \"p90\": %lld, \"p99\": %lld}",
				   n, at(0.5), at(0.9), at(0.99));
		};
		fence_json = fmt("{\"bin_ns\": %" PRIu64 ", \"first_round\": %s, "
				 "\"other_rounds\": %s, \"mean\": %.1f, \"max\": %" PRIu64 "}",
				 FH_NS, pct(0).c_str(), pct(1).c_str(),
				 completions ? (double)fsum / completions : 0.0,
				 fmax);
	}
	// The three virtual addresses of the segment (README, H1: the readers
	// use a second mapping, an alias in PTX terms, section 8.6).
	const void *va_reg = m.host, *va_rd = m.host2;
	const unsigned long long va_dev = (unsigned long long)m.dev;
	int attr_hostptr = -1, attr_pageable = -1;
	{
		CUdevice d0;
		STEP0B_CU(cuDeviceGet(&d0, 0));
		cuDeviceGetAttribute(&attr_hostptr,
				     CU_DEVICE_ATTRIBUTE_CAN_USE_HOST_POINTER_FOR_REGISTERED_MEM,
				     d0);
		cuDeviceGetAttribute(&attr_pageable,
				     CU_DEVICE_ATTRIBUTE_PAGEABLE_MEMORY_ACCESS, d0);
	}
	std::string logs = "[";
	for (size_t i = 0; i < tot.log.size(); i++)
		logs += fmt("%s\"%s\"", i ? ", " : "", tot.log[i].c_str());
	logs += "]";
	const std::string result = fmt(
		"{\"variant\": \"%s\", \"layout\": \"%s\", \"G\": %u, \"chunks\": %u, "
		"\"va_registered\": \"%p\", \"va_device\": \"0x%llx\", \"va_reader\": \"%p\", "
		"\"attr_can_use_host_pointer_for_registered_mem\": %d, \"attr_pageable_memory_access\": %d, "
		"\"warps\": %u, \"S\": %u, \"interleaved\": %d, \"readers\": %d, "
		"\"reverse_read\": %d, \"poison\": %d, \"chunk_rounds_per_launch\": %" PRIu64
		", \"records_requested\": %" PRIu64 ", \"rounds_per_chunk\": %" PRIu64
		", \"warp_rounds_published\": %" PRIu64 ", \"completions\": %" PRIu64
		", \"chunk_rounds_verified\": %" PRIu64 ", \"records_verified\": %" PRIu64
		", \"violating_rounds\": %" PRIu64 ", \"violations_in_first_round_of_launch\": %" PRIu64
		", \"bad_words\": %" PRIu64 ", \"bad_words_completer_warp\": %" PRIu64
		", \"bad_words_other_warps\": %" PRIu64 ", \"stale_words\": %" PRIu64
		", \"poison_words\": %" PRIu64 ", \"future_words\": %" PRIu64
		", \"other_words\": %" PRIu64 ", \"rereads\": %" PRIu64 ", \"healed\": %" PRIu64
		", \"persistent\": %" PRIu64 ", \"heal_ms_max\": %d, \"flag_anomalies\": %" PRIu64
		", \"dead_warps\": %" PRIu64 ", \"cap_hits\": %" PRIu64 ", \"deadline_hits\": %" PRIu64
		", \"fence_ns\": %s, \"launches\": %d, \"budget_hit\": %s, \"gpu_wall_s\": %.3f"
		", \"rate_chunk_rounds_per_s\": %.1f, \"rate_records_per_s\": %.1f, \"log\": %s}",
		VNAMES[v], o.layout.c_str(), o.G, h.nchunks, va_reg, va_dev, va_rd,
		attr_hostptr, attr_pageable,
		nwarps, h.S,
		o.contiguous ? 0 : 1, nreaders, o.forward ? 0 : 1, o.no_poison ? 0 : 1,
		o.chunk_rounds, o.records, rpc, warp_rounds, completions,
		tot.chunk_rounds, tot.records, tot.viol_rounds, tot.viol_first_round,
		tot.bad_words, tot.bad_words_completer, tot.bad_words_others,
		tot.stale, tot.poison, tot.future, tot.other, tot.rereads, tot.healed,
		tot.persistent, tot.heal_ms_max, tot.anomalies, dead, caps, lates,
		fence_json.c_str(), launches, budget_hit ? "true" : "false", t_gpu,
		tot.chunk_rounds / std::max(t_gpu, 1e-9),
		tot.records / std::max(t_gpu, 1e-9), logs.c_str());
	cudaFree(g.cnt);
	cudaFree(g.status);
	cudaFree(g.fhist);
	step0b_regmem_destroy(m);
	printf("%s\n", result.c_str());
	if (!o.out.empty()) {
		FILE *f = fopen(o.out.c_str(), "w");
		if (f) {
			fprintf(f, "%s\n", result.c_str());
			fclose(f);
		}
	}
	return 0;
}

// c2.cuh: the mechanism under test (check C2, step 0b).
//
// Warp claim (the task's mechanism; pools C2_BMSM .. C2_STKGLOB_NV):
//   entry (c2_entry, __noinline__ like a bpftime helper):
//     mask = __activemask()   (X = C2_X_CMASK: the lanes that exist in the warp, from blockDim)
//     leader = lowest lane of mask; n = popc(mask)
//     leader: s = pop a free slot (bitmap atom.or, or versioned stack CAS); none -> counted miss
//     leader: old = atom.add(live[s], 1 << 32 | n)    (gen = old.gen + 1; release in ORD)
//     s, gen -> every lane with __shfl_sync(mask, ., leader)
//     ORD: every lane waits (capped) until ld.acquire(live[s]).gen == gen
//     every lane writes its owner tag tags[s * 32 + lane]; the lane keeps (s, gen, tag) locally
//   exit (c2_exit, per lane):
//     reads its owner tag back (mismatch counted)
//     LANE/CMASK: old = atom.add(live[s], -1) (acq_rel in ORD); (u32)old == 1 -> this lane is the
//           last: release the slot (bitmap atom.and / stack CAS, release in ORD)
//     AGG (not a decision variant): __match_any_sync on s, __syncwarp(peers), one decrement by
//           the exit group's leader
//     FIRST (control): the first exit call of a claim releases the slot
//     CFIRST (control matched to CMASK): constructed-mask claim, first exit call releases
//
// Lane claim (outside the task's stated mechanism; pools C2_LNSM, C2_LNGLOB): every thread
// claims its own slot with one atom.or on a lane-major bitmap (acquire in ORD) and releases it
// with one atom.and (release in ORD). No warp collective, no count.
//
// ORD = 1 uses only orderings the PTX memory model defines (release/acquire patterns and
// observation order through RMWs on one location, PTX ISA 9.5 sections 8.8, 8.9.2, 8.9.4);
// ORD = 0 uses .relaxed everywhere (index exclusivity still follows from RMW atomicity on one
// word, section 8.10.3, but the hand-off of the slot's data between owners is not ordered).
// Every atomic and every check load is a strong .gpu operation on global memory.
#pragma once
#include "c2_shared.h"

typedef unsigned long long u64;
typedef unsigned int u32;

#define C2_CLAIM_CAP 1024u   // atomics per claim before it gives up (counted miss)
#define C2_PUSH_CAP 65536u   // CAS tries per stack push (a cap hit leaks the slot, counted)
#define C2_SPIN_CAP 512u     // polls of a lane's acquire wait (smoke: seen at once or never)

__constant__ PoolDesc g_pd;

#define PD_BM ((u64 *)g_pd.bm)
#define PD_HEAD ((u64 *)g_pd.head)
#define PD_NEXT ((u32 *)g_pd.next)
#define PD_HDR ((u64 *)g_pd.hdr)
#define PD_TAGS ((u64 *)g_pd.tags)
#define PD_CTR ((u64 *)g_pd.ctr)

#define C2_IS_LANE(P) ((P) >= C2_LNSM)
#define C2_IS_LF(P) ((P) >= C2_LFSM)
#define C2_IS_STACK(P) ((P) >= C2_STKSM && (P) <= C2_STKGLOB_NV)
#define C2_HW_HINT(P)                                                                              \
	((P) == C2_BMSM || (P) == C2_STKSM || (P) == C2_STKSM_NV || (P) == C2_LNSM || (P) == C2_LFSM)
#define C2_VERSIONED(P) ((P) == C2_STKSM || (P) == C2_STKGLOB)

// ---- special registers ----------------------------------------------------------------------
__device__ __forceinline__ u32 r_smid()
{
	u32 r;
	asm volatile("mov.u32 %0, %%smid;" : "=r"(r));
	return r;
}
__device__ __forceinline__ u32 r_warpid()
{
	u32 r;
	asm volatile("mov.u32 %0, %%warpid;" : "=r"(r));
	return r;
}
__device__ __forceinline__ u32 r_laneid()
{
	u32 r;
	asm volatile("mov.u32 %0, %%laneid;" : "=r"(r));
	return r;
}
__device__ __forceinline__ u64 r_gtimer()
{
	u64 t;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	return t;
}

// ---- strong memory operations (inline PTX so the qualifiers are exactly what we mean) --------
// SEM: 0 relaxed, 1 acquire, 2 release, 3 acq_rel. All .gpu scope, .global state space.
__device__ __forceinline__ u64 ld_rlx(const u64 *p)
{
	u64 r;
	asm volatile("ld.relaxed.gpu.global.u64 %0, [%1];" : "=l"(r) : "l"(p) : "memory");
	return r;
}
__device__ __forceinline__ u64 ld_acq(const u64 *p)
{
	u64 r;
	asm volatile("ld.acquire.gpu.global.u64 %0, [%1];" : "=l"(r) : "l"(p) : "memory");
	return r;
}
__device__ __forceinline__ u32 ld_rlx32(const u32 *p)
{
	u32 r;
	asm volatile("ld.relaxed.gpu.global.u32 %0, [%1];" : "=r"(r) : "l"(p) : "memory");
	return r;
}
__device__ __forceinline__ void st_rlx(u64 *p, u64 v)
{
	asm volatile("st.relaxed.gpu.global.u64 [%0], %1;" ::"l"(p), "l"(v) : "memory");
}
__device__ __forceinline__ void st_rlx32(u32 *p, u32 v)
{
	asm volatile("st.relaxed.gpu.global.u32 [%0], %1;" ::"l"(p), "r"(v) : "memory");
}

#define C2_ATOM2(NAME, OP)                                                                         \
	template <int SEM> __device__ __forceinline__ u64 NAME(u64 *p, u64 v)                      \
	{                                                                                          \
		u64 r;                                                                             \
		if constexpr (SEM == 0)                                                            \
			asm volatile("atom.relaxed.gpu.global." OP " %0, [%1], %2;"                  \
				     : "=l"(r) : "l"(p), "l"(v) : "memory");                       \
		else if constexpr (SEM == 1)                                                       \
			asm volatile("atom.acquire.gpu.global." OP " %0, [%1], %2;"                  \
				     : "=l"(r) : "l"(p), "l"(v) : "memory");                       \
		else if constexpr (SEM == 2)                                                       \
			asm volatile("atom.release.gpu.global." OP " %0, [%1], %2;"                  \
				     : "=l"(r) : "l"(p), "l"(v) : "memory");                       \
		else                                                                               \
			asm volatile("atom.acq_rel.gpu.global." OP " %0, [%1], %2;"                  \
				     : "=l"(r) : "l"(p), "l"(v) : "memory");                       \
		return r;                                                                          \
	}
C2_ATOM2(a_add, "add.u64")
C2_ATOM2(a_or, "or.b64")
C2_ATOM2(a_and, "and.b64")
C2_ATOM2(a_exch, "exch.b64")

template <int SEM> __device__ __forceinline__ u32 a_cas32(u32 *p, u32 cmp, u32 v)
{
	u32 r;
	if constexpr (SEM == 0)
		asm volatile("atom.relaxed.gpu.global.cas.b32 %0, [%1], %2, %3;"
			     : "=r"(r) : "l"(p), "r"(cmp), "r"(v) : "memory");
	else
		asm volatile("atom.acquire.gpu.global.cas.b32 %0, [%1], %2, %3;"
			     : "=r"(r) : "l"(p), "r"(cmp), "r"(v) : "memory");
	return r;
}
template <int SEM> __device__ __forceinline__ void st32(u32 *p, u32 v)
{
	if constexpr (SEM == 0)
		asm volatile("st.relaxed.gpu.global.u32 [%0], %1;" ::"l"(p), "r"(v) : "memory");
	else
		asm volatile("st.release.gpu.global.u32 [%0], %1;" ::"l"(p), "r"(v) : "memory");
}
template <int SEM> __device__ __forceinline__ u64 a_cas(u64 *p, u64 cmp, u64 v)
{
	u64 r;
	if constexpr (SEM == 0)
		asm volatile("atom.relaxed.gpu.global.cas.b64 %0, [%1], %2, %3;"
			     : "=l"(r) : "l"(p), "l"(cmp), "l"(v) : "memory");
	else if constexpr (SEM == 2)
		asm volatile("atom.release.gpu.global.cas.b64 %0, [%1], %2, %3;"
			     : "=l"(r) : "l"(p), "l"(cmp), "l"(v) : "memory");
	else
		asm volatile("atom.acq_rel.gpu.global.cas.b64 %0, [%1], %2, %3;"
			     : "=l"(r) : "l"(p), "l"(cmp), "l"(v) : "memory");
	return r;
}

// counters: relaxed atomics (the checker only counts; nothing is ordered by them)
__device__ __forceinline__ void cnt(int i, u64 v = 1)
{
	atomicAdd(PD_CTR + (u64)i * C2_CTR_STRIDE, v);
}

__device__ __forceinline__ u32 hash32(u32 x)
{
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}
// first set bit of fb at or after bit s, cyclically
__device__ __forceinline__ u32 pick_bit(u64 fb, u32 s)
{
	s &= 63;
	const u64 rot = s ? ((fb >> s) | (fb << (64 - s))) : fb;
	return ((u32)(__ffsll((long long)rot) - 1) + s) & 63;
}
__device__ __forceinline__ u32 logical_warp()
{
	return (blockIdx.x * blockDim.x + threadIdx.x) >> 5;
}

// ---- pools ----------------------------------------------------------------------------------
struct PopInfo {
	u32 retries, steal, cap;
};

// One bitmap row (wpp words, `bits` valid bits) starting at `row`; first try at bit `hint`
// (no load before it when `optimistic`). Returns the bit or C2_NONE.
template <int ORD>
__device__ __forceinline__ u32 bm_take(u64 *row, u32 wpp, u32 bits, u32 hint, bool optimistic, u32 &tries,
				       PopInfo &pi)
{
#pragma unroll 1
	for (u32 k = 0; k < wpp; k++) {
		const u32 wi = (hint / 64 + k) % wpp;
		const u32 nb = (wi == wpp - 1) ? (bits - 64 * (wpp - 1)) : 64;
		const u64 valid = nb >= 64 ? ~0ull : ((1ull << nb) - 1);
		u64 *w = row + wi;
		u32 c;
		if (optimistic && k == 0) {
			c = hint % 64; // no load before the first atomic
		} else {
			const u64 fb = ~ld_rlx(w) & valid;
			if (!fb)
				continue;
			c = pick_bit(fb, hint);
		}
#pragma unroll 1
		while (true) {
			if (++tries > C2_CLAIM_CAP) {
				pi.cap = 1;
				return C2_NONE;
			}
			const u64 m = 1ull << c;
			const u64 old = a_or<ORD ? 1 : 0>(w, m); // acquire in ORD
			if (!(old & m))
				return wi * 64 + c;
			pi.retries++;
			const u64 fb = ~old & valid;
			if (!fb)
				break;
			c = pick_bit(fb, c);
		}
	}
	return C2_NONE;
}

// Warp pools: the leader only. Returns a slot index or C2_NONE (miss).
template <int P, int ORD> __device__ __forceinline__ u32 pool_pop(PopInfo &pi)
{
	const u32 np = g_pd.npools;
	u32 tries = 0;
	if constexpr (!C2_IS_STACK(P)) {
		const u32 bpp = g_pd.spp, wpp = g_pd.wpp;
		u32 home, hint;
		if constexpr (C2_HW_HINT(P)) {
			home = r_smid() % np;    // locality hint only
			hint = r_warpid() % bpp; // first bit tried; any other free bit is as good
		} else {
			home = 0;
			hint = hash32(logical_warp()) % bpp;
		}
#pragma unroll 1
		for (u32 t = 0; t < np; t++) {
			u32 p = home + t;
			if (p >= np)
				p -= np;
			const u32 b = bm_take<ORD>(PD_BM + (u64)p * wpp, wpp, bpp, hint, t == 0, tries, pi);
			if (b != C2_NONE) {
				pi.steal = t != 0;
				return p * bpp + b;
			}
			if (pi.cap)
				return C2_NONE;
		}
		return C2_NONE;
	} else {
		u32 home = 0;
		if constexpr (C2_HW_HINT(P))
			home = r_smid() % np;
#pragma unroll 1
		for (u32 t = 0; t < np; t++) {
			u32 p = home + t;
			if (p >= np)
				p -= np;
			u64 *h = PD_HEAD + (u64)p * C2_HEAD_STRIDE;
#pragma unroll 1
			while (true) {
				if (++tries > C2_CLAIM_CAP) {
					pi.cap = 1;
					return C2_NONE;
				}
				const u64 hv = ORD ? ld_acq(h) : ld_rlx(h);
				const u32 idx = (u32)hv;
				if (idx == C2_NONE)
					break; // this stack is empty
				const u32 nx = ld_rlx32(PD_NEXT + idx);
				if (g_pd.aba_ns)
					__nanosleep(g_pd.aba_ns);
				const u64 nv = C2_VERSIONED(P) ? (((hv >> 32) + 1) << 32) | nx : (u64)nx;
				const u64 prev = a_cas<0>(h, hv, nv);
				if (prev == hv) {
					pi.steal = t != 0;
					return idx;
				}
				pi.retries++;
			}
		}
		return C2_NONE;
	}
}

// Warp pools: the releasing lane only.
template <int P, int ORD> __device__ __forceinline__ void pool_push(u32 s)
{
	if constexpr (!C2_IS_STACK(P)) {
		const u32 bpp = g_pd.spp, wpp = g_pd.wpp;
		const u32 p = s / bpp, r = s % bpp;
		const u64 m = 1ull << (r % 64);
		const u64 old = a_and<ORD ? 2 : 0>(PD_BM + (u64)p * wpp + r / 64, ~m); // release in ORD
		if (!(old & m))
			cnt(CT_BAD_CLEAR);
	} else {
		const u32 p = s / g_pd.spp;
		u64 *h = PD_HEAD + (u64)p * C2_HEAD_STRIDE;
#pragma unroll 1
		for (u32 k = 0; k < C2_PUSH_CAP; k++) {
			const u64 hv = ld_rlx(h);
			st_rlx32(PD_NEXT + s, (u32)hv);
			const u64 nv = C2_VERSIONED(P) ? (((hv >> 32) + 1) << 32) | s : (u64)s;
			if (a_cas<ORD ? 2 : 0>(h, hv, nv) == hv) // release in ORD
				return;
		}
		cnt(CT_CAP_PUSH);
	}
}

// Lane pools. Region p has 32 lane rows of g_pd.lw bits (g_pd.wpp words each); a thread slot is
// (p, row, bit) and its storage index is p * spp + bit * 32 + row, so a warp whose lanes all get
// the same bit stores into one contiguous 256-byte row. A thread tries its own lane's row first.
template <int P, int ORD> __device__ __forceinline__ u32 lane_pop(PopInfo &pi)
{
	const u32 np = g_pd.npools, W = g_pd.lw, wpp = g_pd.wpp;
	const u32 lane = r_laneid();
	u32 home, hint;
	if constexpr (C2_HW_HINT(P)) {
		home = r_smid() % np;
		hint = r_warpid() % W;
	} else {
		home = 0;
		hint = hash32(logical_warp()) % W;
	}
	u32 tries = 0;
	if constexpr (C2_IS_LF(P)) {
		// one flag word per thread slot: first the hinted slot, then the own lane's row, then the
		// other rows, then the other regions; busy slots are skipped after a plain strong load
		u32 *F = (u32 *)g_pd.bm;
#pragma unroll 1
		for (u32 t = 0; t < np; t++) {
			u32 p = home + t;
			if (p >= np)
				p -= np;
#pragma unroll 1
			for (u32 rr = 0; rr < 32; rr++) {
				const u32 row = (lane + rr) & 31;
#pragma unroll 1
				for (u32 k = 0; k < W; k++) {
					u32 b = hint + k;
					if (b >= W)
						b -= W;
					const u32 s = p * g_pd.spp + b * 32 + row;
					if (++tries > C2_CLAIM_CAP) {
						pi.cap = 1;
						return C2_NONE;
					}
					if ((t | rr | k) && ld_rlx32(F + s) != 0)
						continue;
					if (a_cas32<ORD ? 1 : 0>(F + s, 0, 1) == 0) { // acquire in ORD
						pi.steal = (t | rr | k) != 0;
						return s;
					}
					pi.retries++;
				}
			}
		}
		return C2_NONE;
	} else {
#pragma unroll 1
		for (u32 t = 0; t < np; t++) {
			u32 p = home + t;
			if (p >= np)
				p -= np;
#pragma unroll 1
			for (u32 rr = 0; rr < 32; rr++) {
				const u32 row = (lane + rr) & 31;
				u64 *base = PD_BM + ((u64)p * 32 + row) * wpp;
				const u32 b = bm_take<ORD>(base, wpp, W, hint, t == 0 && rr == 0, tries, pi);
				if (b != C2_NONE) {
					pi.steal = (t != 0) || (rr != 0);
					return p * g_pd.spp + b * 32 + row;
				}
				if (pi.cap)
					return C2_NONE;
			}
		}
		return C2_NONE;
	}
}
template <int P, int ORD> __device__ __forceinline__ void lane_push(u32 s)
{
	if constexpr (C2_IS_LF(P)) {
		st32<ORD ? 2 : 0>((u32 *)g_pd.bm + s, 0); // st.release in ORD
		return;
	}
	const u32 spp = g_pd.spp, wpp = g_pd.wpp;
	const u32 p = s / spp, r = s % spp, b = r / 32, row = r % 32;
	const u64 m = 1ull << (b % 64);
	const u64 old = a_and<ORD ? 2 : 0>(PD_BM + ((u64)p * 32 + row) * wpp + b / 64, ~m); // release
	if (!(old & m))
		cnt(CT_BAD_CLEAR);
}

// ---- per-lane state ---------------------------------------------------------------------------
// What every lane keeps between entry and exit (in bpftime, C1 decides where; here a local).
struct St {
	u32 s;   // slot, C2_NONE on a miss
	u32 gen; // the claim's generation (warp claims)
	u64 tag; // this lane's owner tag; checker: claim tag << 12 | n << 6 | lane
};
#define C2_TAG_CLAIM(t) ((t) >> 12)
#define C2_TAG_N(t) ((u32)((t) >> 6) & 63)

template <int P> __device__ __forceinline__ u64 *tag_ptr(u32 s, u32 lane)
{
	if constexpr (C2_IS_LANE(P))
		return PD_TAGS + s;
	else
		return PD_TAGS + (u64)s * C2_LANES + lane;
}

template <int P, int ORD, int CHK> __device__ __forceinline__ void release_slot(u32 s, u64 lanetag, u32 mode)
{
	if constexpr (CHK) {
		const u64 claim = C2_TAG_CLAIM(lanetag);
		if (a_cas<0>(PD_HDR + (u64)s * C2_HDR_WORDS + 2, claim, 0) != claim)
			cnt(CT_BAD_REL);
		cnt(CT_RELEASES);
	} else if (mode & 1) {
		cnt(CT_RELEASES);
	}
	if constexpr (C2_IS_LANE(P))
		lane_push<P, ORD>(s);
	else
		pool_push<P, ORD>(s);
}

// ---- entry -------------------------------------------------------------------------------------
// The mask of lanes that exist in the calling warp, from the documented partition of a block into
// warps (consecutive thread ids, the first warp holding thread 0).
__device__ __forceinline__ u32 warp_mask_by_construction()
{
	const u32 tid = threadIdx.x + blockDim.x * (threadIdx.y + blockDim.y * threadIdx.z);
	const u32 bsz = blockDim.x * blockDim.y * blockDim.z;
	const u32 present = bsz - (tid & ~31u);
	return present >= 32 ? 0xffffffffu : ((1u << present) - 1);
}

template <int P, int ORD, int X, int CHK> __device__ __noinline__ St c2_entry(u32 mode)
{
	const u32 lane = r_laneid();
	St st;
	st.s = C2_NONE;
	st.gen = 0;
	st.tag = 0;
	if constexpr (C2_IS_LANE(P)) {
		PopInfo pi = { 0, 0, 0 };
		const u32 s = lane_pop<P, ORD>(pi);
		if (pi.cap)
			cnt(CT_CAP_CLAIM);
		if (s == C2_NONE) {
			cnt(CT_MISSES);
			cnt(CT_MISS_LANES);
			return st;
		}
		u64 claim = 0;
		if constexpr (CHK) {
			claim = atomicAdd(PD_CTR + (u64)CT_TAGCTR * C2_CTR_STRIDE, 1ull) + 1;
			if (a_exch<0>(PD_HDR + (u64)s * C2_HDR_WORDS + 2, claim) != 0)
				cnt(CT_DBL_CLAIM);
			cnt(CT_CLAIMS);
			cnt(CT_CLAIM_LANES);
			if (pi.steal)
				cnt(CT_STEAL);
			if (pi.retries)
				cnt(CT_RETRY, pi.retries);
		} else if (mode & 1) {
			cnt(CT_CLAIMS);
			cnt(CT_CLAIM_LANES);
		}
		const u64 lt = CHK ? ((claim << 12) | (1ull << 6) | lane)
				   : (u64)(blockIdx.x * blockDim.x + threadIdx.x) + 1;
		st_rlx(tag_ptr<P>(s, lane), lt);
		st.s = s;
		st.tag = lt;
		if constexpr (X == C2_X_FIRST) // control: release now, keep using the slot
			release_slot<P, ORD, CHK>(s, lt, mode);
		return st;
	} else {
		u32 mask;
		if constexpr (X == C2_X_CMASK || X == C2_X_CFIRST)
			mask = warp_mask_by_construction();
		else
			mask = __activemask();
		const u32 leader = __ffs(mask) - 1;
		const u32 n = __popc(mask);
		u32 s = C2_NONE, gen = 0;
		u64 claim = 0;
		if (lane == leader) {
			PopInfo pi = { 0, 0, 0 };
			s = pool_pop<P, ORD>(pi);
			if (s != C2_NONE) {
				u64 *live = PD_HDR + (u64)s * C2_HDR_WORDS;
				if constexpr (CHK) {
					// checker: the ledger entry is written BEFORE the release add, so each
					// lane's ld.acquire of that add also orders the ledger value for the lane
					// (ORD); otherwise a lane could read the previous 0 without any fault of
					// the mechanism (a false freed_live / bad_rel)
					claim = atomicAdd(PD_CTR + (u64)CT_TAGCTR * C2_CTR_STRIDE, 1ull) + 1;
					if (a_exch<0>(live + 2, claim) != 0)
						cnt(CT_DBL_CLAIM);
				}
				const u64 old = a_add<ORD ? 2 : 0>(live, (1ull << 32) | n); // release in ORD
				gen = (u32)(old >> 32) + 1;
				if constexpr (CHK) {
					cnt(CT_CLAIMS);
					cnt(CT_CLAIM_LANES, n);
					if (pi.steal)
						cnt(CT_STEAL);
					if (pi.retries)
						cnt(CT_RETRY, pi.retries);
				} else if (mode & 1) {
					cnt(CT_CLAIMS);
					cnt(CT_CLAIM_LANES, n);
				}
			} else {
				cnt(CT_MISSES);
				cnt(CT_MISS_LANES, n);
			}
			if (pi.cap)
				cnt(CT_CAP_CLAIM);
		}
		s = __shfl_sync(mask, s, leader);
		if (s == C2_NONE)
			return st;
		gen = __shfl_sync(mask, gen, leader);
		if constexpr (CHK)
			claim = __shfl_sync(mask, claim, leader);
		u64 *live = PD_HDR + (u64)s * C2_HDR_WORDS;
		if constexpr (ORD) {
			// Acquire this claim's release-add (or a later RMW in its observation order): it
			// orders this lane's slot accesses after the previous owner's (no warp ordering).
			u32 it = 0;
#pragma unroll 1
			for (; it < C2_SPIN_CAP; it++)
				if ((u32)(ld_acq(live) >> 32) == gen)
					break;
			if (it == C2_SPIN_CAP)
				cnt(CT_CAP_SPIN);
			else if (it)
				cnt(CT_SPIN_EXTRA, it);
		}
		u64 lt;
		if constexpr (CHK)
			lt = (claim << 12) | ((u64)n << 6) | lane;
		else
			lt = (u64)(blockIdx.x * blockDim.x + threadIdx.x) + 1;
		st_rlx(tag_ptr<P>(s, lane), lt);
		st.s = s;
		st.gen = gen;
		st.tag = lt;
		return st;
	}
}

// ---- checkpoint (checker only, inlined into the kernels) ---------------------------------------
template <int P> __device__ __forceinline__ void c2_check(const St &st)
{
	if (st.s == C2_NONE)
		return;
	const u64 lg = ld_rlx(PD_HDR + (u64)st.s * C2_HDR_WORDS + 2);
	if (lg == 0)
		cnt(CT_FREED_LIVE);
	else if (lg != C2_TAG_CLAIM(st.tag))
		cnt(CT_FOREIGN);
	if (ld_rlx(tag_ptr<P>(st.s, r_laneid())) != st.tag)
		cnt(CT_LANETAG_BAD);
}

// ---- exit --------------------------------------------------------------------------------------
// step (checker only): an id of the exit call site and time; the first decrementer of a claim
// stores it, the releasing lane compares (no warp collective in the checker).
template <int P, int ORD, int X, int CHK> __device__ __noinline__ void c2_exit(St st, u32 mode, u32 step)
{
	const u32 s = st.s;
	const u32 lane = r_laneid();
	u32 peers = 0;
	if constexpr (X == C2_X_AGG && !C2_IS_LANE(P)) {
		// all active lanes, also those without a slot, take part in the match
		peers = __match_any_sync(__activemask(), s);
	}
	if (s == C2_NONE)
		return;
	u64 *hdr = PD_HDR + (u64)s * C2_HDR_WORDS;
	if (ld_rlx(tag_ptr<P>(s, lane)) != st.tag)
		cnt(CT_TAG_BAD);
	if constexpr (CHK) {
		const u64 lg = ld_rlx(hdr + 2);
		if (lg == 0)
			cnt(CT_FREED_LIVE);
		else if (lg != C2_TAG_CLAIM(st.tag))
			cnt(CT_FOREIGN);
		cnt(CT_EXIT_LANES);
	} else if (mode & 1) {
		cnt(CT_EXIT_LANES);
	}
	if constexpr (C2_IS_LANE(P)) {
		if constexpr (X != C2_X_FIRST)
			release_slot<P, ORD, CHK>(s, st.tag, mode);
	} else if constexpr (X == C2_X_LANE || X == C2_X_CMASK) {
		const u64 old = a_add<ORD ? 3 : 0>(hdr, ~0ull); // -1; acq_rel in ORD
		if constexpr (CHK) {
			if ((u32)old == C2_TAG_N(st.tag) && (u32)old > 1)
				st_rlx(hdr + 3, (u64)step + 1); // first exit call of this claim
			else if ((u32)old == 1 && C2_TAG_N(st.tag) > 1 && ld_rlx(hdr + 3) != (u64)step + 1)
				cnt(CT_SPLIT_EXIT); // first and last exits came from different calls
		}
		if ((u32)old == 1)
			release_slot<P, ORD, CHK>(s, st.tag, mode);
	} else if constexpr (X == C2_X_AGG) {
		__syncwarp(peers); // bar.warp.sync: the instruction-level ordering guarantee only
		const u32 k = __popc(peers);
		if (lane == (u32)(__ffs(peers) - 1)) {
			const u64 old = a_add<ORD ? 3 : 0>(hdr, (u64)0 - k);
			if constexpr (CHK) {
				if ((u32)old == C2_TAG_N(st.tag) && (u32)old > k)
					st_rlx(hdr + 3, (u64)step + 1);
				else if ((u32)old == k && k < C2_TAG_N(st.tag) && ld_rlx(hdr + 3) != (u64)step + 1)
					cnt(CT_SPLIT_EXIT);
			}
			if ((u32)old == k)
				release_slot<P, ORD, CHK>(s, st.tag, mode);
		}
	} else { // C2_X_FIRST / C2_X_CFIRST, the controls: the first exit call releases
		const u64 old = a_exch<0>(hdr + 1, (u64)st.gen);
		if (old != (u64)st.gen)
			release_slot<P, ORD, CHK>(s, st.tag, mode);
	}
}

// ---- %warpid-addressed baseline (no claim) and the empty probe ---------------------------------
// The same storage layout and tag operations as the lane pools (index = (sm * W + warpid) * 32 + lane).
__device__ __noinline__ St c2_entry_wid(u32 mode)
{
	const u32 s = (r_smid() % g_pd.wid_S) * g_pd.wid_W + r_warpid() % g_pd.wid_W;
	const u32 lane = r_laneid();
	const u64 lt = (u64)(blockIdx.x * blockDim.x + threadIdx.x) + 1;
	st_rlx(PD_TAGS + (u64)s * C2_LANES + lane, lt);
	St st;
	st.s = s;
	st.gen = 0;
	st.tag = lt;
	return st;
}
__device__ __noinline__ void c2_exit_wid(St st, u32 mode, u32 step)
{
	if (ld_rlx(PD_TAGS + (u64)st.s * C2_LANES + r_laneid()) != st.tag)
		cnt(CT_TAG_BAD);
}
__device__ __noinline__ St c2_entry_probe(u32 mode)
{
	asm volatile("" ::: "memory");
	St st;
	st.s = mode;
	st.gen = 0;
	st.tag = 0;
	return st;
}
__device__ __noinline__ void c2_exit_probe(St st, u32 mode, u32 step)
{
	asm volatile("" ::"r"(st.s) : "memory");
}

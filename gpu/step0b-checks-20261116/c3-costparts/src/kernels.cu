// c3-costparts kernels (step 0b, check C3): a copy of step 0's
// e2-cost/src/kernels.cu, unchanged above the line "C3 additions", plus the
// cases of C3 (b) slot addressing and (d) fence kinds at the end.
//
// e2-cost kernels: placement and atomic cost (step 0, check 2).
// Built with nvcc -cubin for sm_87 and launched with cuLaunchKernel, the same
// launch path as the 1512 reference module (ref/build_ref.py).
//
// Every kernel has the signature
//   (u64 *buf, u64 mask, u32 R, u32 k, u64 *aux, u32 mode)
//   buf   registered host memory (cuMemHostGetDevicePointer) or cuMemAlloc
//   mask  buffer size in u64 words minus 1 (a power of two)
//   R     operations per thread (loop trip count)
//   k     participating warps per SM (atom_sm / cas_sm only: %warpid < k)
//   aux   device memory: [0] CAS cap hits, [1] sink, [64+s] participants on
//         SM s (validation mode), [128] log count, [256..] log entries
//   mode  bit 0: validation (count and log participants; never timed)
//
// Stores and records go to a fresh address in every iteration; counters stay
// on one word. Every store, atomic and fence is inline PTX, so nvcc keeps one
// instruction per source operation (checked in SASS by sass_check.py).
#include <cstdint>

#define FULL_MASK 0xffffffffu
#define CAS_CAP 65536u
#define AUX_CAPHITS 0
#define AUX_SINK 1
#define AUX_PART 64
#define AUX_LOGN 128
#define AUX_LOG 256
#define AUX_LOG_MAX ((8u << 20) / 8 - AUX_LOG) / 4
#define SINK_MAGIC 0x5a5aa5a5f00dfaceULL
// composite record path: per-SM ring in buf, after the per-SM words
#define RING_BASE_WORDS 8192ull
#define RING_SLOTS 4096ull
#define RING_SLOT_WORDS 8ull // 64 B: 4 words of record, lap word at +4

__device__ __forceinline__ unsigned hw_smid()
{
	unsigned r;
	asm volatile("mov.u32 %0, %%smid;" : "=r"(r));
	return r;
}
__device__ __forceinline__ unsigned hw_warpid()
{
	unsigned r;
	asm volatile("mov.u32 %0, %%warpid;" : "=r"(r));
	return r;
}
__device__ __forceinline__ unsigned hw_laneid()
{
	unsigned r;
	asm volatile("mov.u32 %0, %%laneid;" : "=r"(r));
	return r;
}
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
// __threadfence_system() is membar.sys as well; inline PTX keeps it visible.
__device__ __forceinline__ void membar_sys()
{
	asm volatile("membar.sys;" ::: "memory");
}
__device__ __forceinline__ uint64_t atom_add(uint64_t *p, uint64_t v)
{
	uint64_t r;
	asm volatile("atom.global.add.u64 %0, [%1], %2;"
		     : "=l"(r)
		     : "l"(p), "l"(v)
		     : "memory");
	return r;
}
__device__ __forceinline__ uint64_t atom_add_sys(uint64_t *p, uint64_t v)
{
	uint64_t r;
	asm volatile("atom.sys.global.add.u64 %0, [%1], %2;"
		     : "=l"(r)
		     : "l"(p), "l"(v)
		     : "memory");
	return r;
}
__device__ __forceinline__ void red_add(uint64_t *p, uint64_t v)
{
	asm volatile("red.global.add.u64 [%0], %1;" ::"l"(p), "l"(v)
		     : "memory");
}
__device__ __forceinline__ uint64_t atom_cas(uint64_t *p, uint64_t c,
					     uint64_t v)
{
	uint64_t r;
	asm volatile("atom.global.cas.b64 %0, [%1], %2, %3;"
		     : "=l"(r)
		     : "l"(p), "l"(c), "l"(v)
		     : "memory");
	return r;
}
__device__ __forceinline__ uint64_t ld_volatile(const uint64_t *p)
{
	uint64_t r;
	asm volatile("ld.volatile.global.u64 %0, [%1];"
		     : "=l"(r)
		     : "l"(p)
		     : "memory");
	return r;
}

struct Ids {
	uint64_t tid, nthreads, warp, nwarps;
	unsigned lane;
};
__device__ __forceinline__ Ids ids()
{
	Ids i;
	i.tid = (uint64_t)blockIdx.x * blockDim.x + threadIdx.x;
	i.nthreads = (uint64_t)gridDim.x * blockDim.x;
	i.warp = i.tid >> 5;
	i.nwarps = i.nthreads >> 5;
	i.lane = threadIdx.x & 31u; // 1-D blocks of a multiple of 32
	return i;
}
__device__ __forceinline__ void keep(uint64_t acc, uint64_t *aux)
{
	if (acc == SINK_MAGIC)
		aux[AUX_SINK] = acc;
}

#define SIG                                                                    \
	(uint64_t * buf, uint64_t mask, uint32_t R, uint32_t k, uint64_t * aux, \
	 uint32_t mode)

// ---- baseline --------------------------------------------------------------
extern "C" __global__ void k_empty SIG
{
}

// ---- timing gate (not a case) ------------------------------------------------
// Launched before the first event of a timed batch: thread 0 polls a
// host-mapped word until the host has queued the whole batch and set it, so
// that the events time GPU execution and not the host's launch rate (about
// 3.5 us per cuLaunchKernel, which bounded the 64-thread and full-occupancy
// empty kernels). Capped by globaltimer and an iteration count; a cap hit is
// counted in hostw[1] and marks the batch.
#define HOLD_ITER_CAP (1u << 24)
extern "C" __global__ void k_hold(uint64_t *hostw, uint64_t cap_ns)
{
	if (threadIdx.x != 0)
		return;
	const uint64_t t0 = gtimer();
#pragma unroll 1
	for (uint32_t it = 0; it < HOLD_ITER_CAP; it++) {
		if (ld_volatile(hostw) != 0)
			return;
		if (gtimer() - t0 > cap_ns)
			break;
		__nanosleep(256);
	}
	st_u64(hostw + 1, ld_volatile(hostw + 1) + 1);
}

// ---- plain stores ----------------------------------------------------------
template <int F> __device__ __forceinline__ void st_lane32 SIG
{
	const Ids i = ids();
#pragma unroll 1
	for (uint32_t r = 0; r < R; r++) {
		st_u64(buf + ((r * i.nthreads + i.tid) & mask), i.tid + r + 1);
		if (F)
			membar_sys();
	}
}
template <int F> __device__ __forceinline__ void st_lane1 SIG
{
	const Ids i = ids();
	if (i.lane != 0)
		return;
#pragma unroll 1
	for (uint32_t r = 0; r < R; r++) {
		st_u64(buf + ((r * i.nthreads + i.tid) & mask), i.tid + r + 1);
		if (F)
			membar_sys();
	}
}
// ---- 32-byte records -------------------------------------------------------
template <int F> __device__ __forceinline__ void rec_lane0 SIG
{
	const Ids i = ids();
	if (i.lane != 0)
		return;
	const uint64_t rmask = mask >> 2;
#pragma unroll 1
	for (uint32_t r = 0; r < R; r++) {
		uint64_t *p = buf + (((r * i.nwarps + i.warp) & rmask) << 2);
		const uint64_t v = (i.warp + r + 1) << 2;
		st_u64(p + 0, v | 0);
		st_u64(p + 1, v | 1);
		st_u64(p + 2, v | 2);
		st_u64(p + 3, v | 3);
		if (F)
			membar_sys();
	}
}
template <int F> __device__ __forceinline__ void rec_lane4 SIG
{
	const Ids i = ids();
	if (i.lane >= 4)
		return;
	const uint64_t rmask = mask >> 2;
#pragma unroll 1
	for (uint32_t r = 0; r < R; r++) {
		uint64_t *p = buf + (((r * i.nwarps + i.warp) & rmask) << 2);
		st_u64(p + i.lane, ((i.warp + r + 1) << 2) | i.lane);
		if (F)
			membar_sys();
	}
}
template <int F> __device__ __forceinline__ void rec_thread SIG
{
	const Ids i = ids();
	const uint64_t rmask = mask >> 2;
#pragma unroll 1
	for (uint32_t r = 0; r < R; r++) {
		uint64_t *p = buf + (((r * i.nthreads + i.tid) & rmask) << 2);
		const uint64_t v = (i.tid + r + 1) << 2;
		st_u64(p + 0, v | 0);
		st_u64(p + 1, v | 1);
		st_u64(p + 2, v | 2);
		st_u64(p + 3, v | 3);
		if (F)
			membar_sys();
	}
}
// ---- atomics ---------------------------------------------------------------
template <int F> __device__ __forceinline__ void atom_lane SIG
{
	const Ids i = ids();
	uint64_t *p = buf + (i.tid & mask);
	uint64_t acc = 0;
#pragma unroll 1
	for (uint32_t r = 0; r < R; r++) {
		acc += atom_add(p, 1);
		if (F)
			membar_sys();
	}
	keep(acc, aux);
}
template <int F> __device__ __forceinline__ void atom_warp_all SIG
{
	const Ids i = ids();
	uint64_t *p = buf + ((i.warp << 2) & mask); // one word per 32 B
	uint64_t acc = 0;
#pragma unroll 1
	for (uint32_t r = 0; r < R; r++) {
		acc += atom_add(p, 1);
		if (F)
			membar_sys();
	}
	keep(acc, aux);
}
template <int F> __device__ __forceinline__ void atom_warp_leader SIG
{
	const Ids i = ids();
	if (i.lane != 0)
		return;
	uint64_t *p = buf + ((i.warp << 2) & mask);
	uint64_t acc = 0;
#pragma unroll 1
	for (uint32_t r = 0; r < R; r++) {
		acc += atom_add(p, 1);
		if (F)
			membar_sys();
	}
	keep(acc, aux);
}
template <int F> __device__ __forceinline__ void atom_warp_leader_sys SIG
{
	const Ids i = ids();
	if (i.lane != 0)
		return;
	uint64_t *p = buf + ((i.warp << 2) & mask);
	uint64_t acc = 0;
#pragma unroll 1
	for (uint32_t r = 0; r < R; r++) {
		acc += atom_add_sys(p, 1);
		if (F)
			membar_sys();
	}
	keep(acc, aux);
}
template <int F> __device__ __forceinline__ void red_warp_leader SIG
{
	const Ids i = ids();
	if (i.lane != 0)
		return;
	uint64_t *p = buf + ((i.warp << 2) & mask);
#pragma unroll 1
	for (uint32_t r = 0; r < R; r++) {
		red_add(p, 1);
		if (F)
			membar_sys();
	}
}
// One word per SM (smid % 64, 128 B apart), hit by lane 0 of every warp whose
// %warpid is below k. Validation mode counts and logs the participants.
__device__ __forceinline__ void log_participant(uint64_t *aux, unsigned sm,
						unsigned wid, uint64_t t0,
						uint64_t t1)
{
	atom_add(aux + AUX_PART + (sm & 63), 1);
	const uint64_t n = atom_add(aux + AUX_LOGN, 1);
	if (n < AUX_LOG_MAX) {
		uint64_t *e = aux + AUX_LOG + n * 4;
		e[0] = ((uint64_t)sm << 32) | wid;
		e[1] = t0;
		e[2] = t1;
		e[3] = (uint64_t)blockIdx.x;
	}
}
template <int F> __device__ __forceinline__ void atom_sm SIG
{
	const Ids i = ids();
	const unsigned wid = hw_warpid();
	if (i.lane != 0 || wid >= k)
		return;
	const unsigned sm = hw_smid();
	uint64_t *p = buf + ((uint64_t)(sm & 63) << 4);
	const uint64_t t0 = (mode & 1) ? gtimer() : 0;
	uint64_t acc = 0;
#pragma unroll 1
	for (uint32_t r = 0; r < R; r++) {
		acc += atom_add(p, 1);
		if (F)
			membar_sys();
	}
	keep(acc, aux);
	if (mode & 1)
		log_participant(aux, sm, wid, t0, gtimer());
}
template <int F> __device__ __forceinline__ void cas_sm SIG
{
	const Ids i = ids();
	const unsigned wid = hw_warpid();
	if (i.lane != 0 || wid >= k)
		return;
	const unsigned sm = hw_smid();
	uint64_t *p = buf + ((uint64_t)(sm & 63) << 4);
	const uint64_t t0 = (mode & 1) ? gtimer() : 0;
	uint64_t acc = 0;
#pragma unroll 1
	for (uint32_t r = 0; r < R; r++) {
		uint64_t old = ld_volatile(p);
		uint32_t it = 0;
#pragma unroll 1
		for (; it < CAS_CAP; it++) {
			const uint64_t prev = atom_cas(p, old, old + 1);
			if (prev == old)
				break;
			old = prev;
		}
		if (it == CAS_CAP)
			atom_add(aux + AUX_CAPHITS, 1);
		acc += old;
		if (F)
			membar_sys();
	}
	keep(acc, aux);
	if (mode & 1)
		log_participant(aux, sm, wid, t0, gtimer());
}
// Composite record path (design D's per-SM ring, not used by the decision
// rules): lane 0 claims a slot with a fetch-add on the SM's cursor, lanes 0-3
// write the 32-byte record, __syncwarp, lane 0 fences (F) and stores the
// slot's lap word with st.relaxed.sys.
template <int F> __device__ __forceinline__ void comp_rec SIG
{
	const Ids i = ids();
	const unsigned sm = hw_smid() & 63;
	uint64_t *cursor = buf + ((uint64_t)sm << 4);
	uint64_t *ring = buf + RING_BASE_WORDS +
			 (uint64_t)sm * RING_SLOTS * RING_SLOT_WORDS;
#pragma unroll 1
	for (uint32_t r = 0; r < R; r++) {
		uint64_t pos = 0;
		if (i.lane == 0)
			pos = atom_add(cursor, 1);
		pos = __shfl_sync(FULL_MASK, pos, 0);
		uint64_t *slot = ring + (pos % RING_SLOTS) * RING_SLOT_WORDS;
		if (i.lane < 4)
			st_u64(slot + i.lane, ((i.warp + r + 1) << 2) | i.lane);
		__syncwarp(FULL_MASK);
		if (i.lane == 0) {
			if (F)
				membar_sys();
			st_relaxed_sys(slot + 4, pos + 1);
		}
	}
}

#define INST(name)                                                             \
	extern "C" __global__ void k_##name##_f0 SIG                           \
	{                                                                      \
		name<0>(buf, mask, R, k, aux, mode);                           \
	}                                                                      \
	extern "C" __global__ void k_##name##_f1 SIG                           \
	{                                                                      \
		name<1>(buf, mask, R, k, aux, mode);                           \
	}

INST(st_lane32)
INST(st_lane1)
INST(rec_lane0)
INST(rec_lane4)
INST(rec_thread)
INST(atom_lane)
INST(atom_warp_all)
INST(atom_warp_leader)
INST(atom_warp_leader_sys)
INST(red_warp_leader)
INST(atom_sm)
INST(cas_sm)
INST(comp_rec)

// ============================================================================
// C3 additions (step 0b). Everything above is step 0's e2-cost kernels.cu.
// ============================================================================
#define AUX_MISS 2 // C3 (b): exit tag checks that did not match
#define AUX_OOR 3 // C3 (b): slot index out of range (fallback taken)
#define AUX_HITS 4 // C3 (b): exit tag checks that matched (validation only)
#define TIDW_SLOTS 16384u // C3 (b): tid-folded table, power of two >= 12,288

__device__ __forceinline__ unsigned hw_nsmid()
{
	unsigned r;
	asm volatile("mov.u32 %0, %%nsmid;" : "=r"(r));
	return r;
}
__device__ __forceinline__ unsigned hw_nwarpid()
{
	unsigned r;
	asm volatile("mov.u32 %0, %%nwarpid;" : "=r"(r));
	return r;
}
__device__ __forceinline__ void st_v2(uint64_t *p, uint64_t a, uint64_t b)
{
	asm volatile("st.global.v2.u64 [%0], {%1, %2};" ::"l"(p), "l"(a),
		     "l"(b)
		     : "memory");
}
template <int CG>
__device__ __forceinline__ void ld_v2(const uint64_t *p, uint64_t &a,
				      uint64_t &b)
{
	if (CG)
		asm volatile("ld.global.cg.v2.u64 {%0, %1}, [%2];"
			     : "=l"(a), "=l"(b)
			     : "l"(p)
			     : "memory");
	else
		asm volatile("ld.global.v2.u64 {%0, %1}, [%2];"
			     : "=l"(a), "=l"(b)
			     : "l"(p)
			     : "memory");
}

// ---- (b) slot addressing ----------------------------------------------------
// A 16-byte slot {owner tag, state} per thread. ADDR selects the index:
//   0  hardware position: (%smid x %nwarpid + %warpid) x 32 + %laneid, out of
//      (%nsmid x %nwarpid x 32) slots (12,288 here, 192 KiB); an index out of
//      range is counted (AUX_OOR) and the slot is not used
//   1  the global 1-D thread id (T slots; control without special registers)
//   2  the thread id folded to TIDW_SLOTS (256 KiB, about the hardware table's
//      footprint; control without special registers)
// EXIT: 0 entry only (index + 16-B store {tag, 0}); 1 entry, then the exit
// side as a separate probe would do it: index recomputed from the ids, 16-B
// load, tag compared, mismatch counted (AUX_MISS); 2 the same with
// ld.global.cg (L2, not L1) at exit. The exit side's instructions are all
// there (SASS-checked), but ptxas schedules its %smid/%warpid reads before
// the entry store, so the ids are not re-read after the entry: this times
// the instructions and cannot observe an id change between entry and exit.
// mode bit 1 (validation only): the exit side expects tag ^ 1, so every check
// must miss (the control that shows the check can fail).
template <int ADDR>
__device__ __forceinline__ bool slot_index(const Ids &i, uint64_t mask,
					   uint32_t &idx)
{
	uint32_t cap;
	if (ADDR == 0) {
		const unsigned sm = hw_smid(), wid = hw_warpid(),
			       lane = hw_laneid();
		const unsigned nw = hw_nwarpid(), ns = hw_nsmid();
		idx = (sm * nw + wid) * 32u + lane;
		cap = ns * nw * 32u;
	} else if (ADDR == 1) {
		idx = (uint32_t)i.tid;
		cap = (uint32_t)((mask + 1) >> 1);
	} else {
		idx = (uint32_t)i.tid & (TIDW_SLOTS - 1);
		cap = TIDW_SLOTS;
	}
	return idx < cap;
}
template <int ADDR, int EXIT> __device__ __forceinline__ void scratch SIG
{
	const Ids i = ids();
	const uint64_t ktag = (uint64_t)(k + 1) << 48;
	uint64_t acc = 0, miss = 0, oor = 0, hits = 0;
#pragma unroll 1
	for (uint32_t r = 0; r < R; r++) {
		const uint64_t tag = ktag | ((uint64_t)(r & 0xff) << 40) |
				     (i.tid & 0xffffffffffull);
		uint32_t idx;
		if (slot_index<ADDR>(i, mask, idx))
			st_v2(buf + 2ull * idx, tag, 0);
		else
			oor++;
		if (EXIT) {
			uint32_t idx2;
			const uint64_t want = (mode & 2) ? (tag ^ 1) : tag;
			if (slot_index<ADDR>(i, mask, idx2)) {
				uint64_t t, v;
				ld_v2<EXIT == 2>(buf + 2ull * idx2, t, v);
				if (t != want) {
					miss++;
				} else {
					hits++;
					acc += v + 1;
				}
			} else {
				oor++;
			}
		}
	}
	if (miss)
		atom_add(aux + AUX_MISS, miss);
	if (oor)
		atom_add(aux + AUX_OOR, oor);
	if (mode & 1)
		atom_add(aux + AUX_HITS, hits);
	keep(acc, aux);
}

// Claim-based form, built from documented primitives instead of the measured
// exclusivity of hardware slots (the PTX ISA promises %warpid unique within a
// CTA only, and %smid/%warpid may change after preemption); whether the
// protocol is sufficient under the memory model is not argued here, this is
// its cost: entry claims the slot with a
// CAS from 0 (free) to the tag and clears the state; a failed claim is
// counted (AUX_CFAIL) and the thread runs without a slot; exit recomputes the
// index, checks the tag (strong 8-B load) and releases the slot with a store
// of 0. ptxas schedules the exit side's %smid/%warpid reads before the
// entry CAS (SASS), so this times the instructions, not an entry-to-exit gap.
// ACQREL = 1: atom.acquire.gpu.cas and st.release.gpu (PTX ISA 8.8 acquire and
// release patterns, so a later owner cannot see this owner's state writes
// land after its own); ACQREL = 0: relaxed claim and release (cost probe).
// mode bit 2 (validation only): the index is halved, so lanes 2j and 2j+1
// collide and one claim of each pair must fail (the control that shows a
// conflict is detected and counted).
// The claim table is the top TIDW_SLOTS x 16 B of buf, which no other item
// writes (they all start at word 0 and stay below 64 MiB), so a slot left
// claimed by another case cannot make the timed claims fail; an index at or
// above TIDW_SLOTS is counted as out of range.
#define AUX_CFAIL 5
template <int ACQREL>
__device__ __forceinline__ uint64_t cas_claim(uint64_t *p, uint64_t tag)
{
	uint64_t r;
	if (ACQREL)
		asm volatile("atom.acquire.gpu.global.cas.b64 %0, [%1], %2, %3;"
			     : "=l"(r)
			     : "l"(p), "l"(0ull), "l"(tag)
			     : "memory");
	else
		asm volatile("atom.relaxed.gpu.global.cas.b64 %0, [%1], %2, %3;"
			     : "=l"(r)
			     : "l"(p), "l"(0ull), "l"(tag)
			     : "memory");
	return r;
}
template <int ACQREL> __device__ __forceinline__ void st_release(uint64_t *p)
{
	if (ACQREL)
		asm volatile("st.release.gpu.global.u64 [%0], %1;" ::"l"(p),
			     "l"(0ull)
			     : "memory");
	else
		asm volatile("st.relaxed.gpu.global.u64 [%0], %1;" ::"l"(p),
			     "l"(0ull)
			     : "memory");
}
__device__ __forceinline__ uint64_t ld_relaxed_gpu(const uint64_t *p)
{
	uint64_t r;
	asm volatile("ld.relaxed.gpu.global.u64 %0, [%1];"
		     : "=l"(r)
		     : "l"(p)
		     : "memory");
	return r;
}
template <int ACQREL> __device__ __forceinline__ void scratch_claim SIG
{
	const Ids i = ids();
	const uint64_t ktag = (uint64_t)(k + 1) << 48;
	uint64_t *tab = buf + (mask + 1) - 2ull * TIDW_SLOTS;
	uint64_t acc = 0, miss = 0, oor = 0, hits = 0, cfail = 0;
#pragma unroll 1
	for (uint32_t r = 0; r < R; r++) {
		const uint64_t tag = ktag | ((uint64_t)(r & 0xff) << 40) |
				     (i.tid & 0xffffffffffull);
		uint32_t idx;
		if (slot_index<0>(i, mask, idx) && idx < TIDW_SLOTS) {
			if (mode & 4)
				idx >>= 1;
			if (cas_claim<ACQREL>(tab + 2ull * idx, tag) == 0)
				st_u64(tab + 2ull * idx + 1, 0);
			else
				cfail++;
		} else {
			oor++;
		}
		uint32_t idx2;
		if (slot_index<0>(i, mask, idx2) && idx2 < TIDW_SLOTS) {
			if (mode & 4)
				idx2 >>= 1;
			// The tag is read with a strong 8-B load (ld.relaxed.gpu), so
			// it is morally strong with the other threads' CAS and
			// release stores on the same 8 B and single-copy atomic
			// (PTX ISA 8.7, 8.10.3); a weak load racing with them has no
			// such guarantee. The state word is this thread's own (weak).
			const uint64_t t = ld_relaxed_gpu(tab + 2ull * idx2);
			uint64_t v;
			asm volatile("ld.global.u64 %0, [%1];"
				     : "=l"(v)
				     : "l"(tab + 2ull * idx2 + 1)
				     : "memory");
			if (t == tag) {
				hits++;
				acc += v + 1;
				st_release<ACQREL>(tab + 2ull * idx2);
			} else {
				miss++;
			}
		} else {
			oor++;
		}
	}
	if (miss)
		atom_add(aux + AUX_MISS, miss);
	if (oor)
		atom_add(aux + AUX_OOR, oor);
	if (cfail)
		atom_add(aux + AUX_CFAIL, cfail);
	if (mode & 1)
		atom_add(aux + AUX_HITS, hits);
	keep(acc, aux);
}

// ---- (d) fence kinds ----------------------------------------------------------
// FK: 1 membar.sys (= fence.sc.sys, __threadfence_system), 2 membar.gl
// (= fence.sc.gpu, __threadfence), 3 fence.acq_rel.gpu, 4 fence.acq_rel.sys.
template <int FK> __device__ __forceinline__ void fence_k()
{
	if (FK == 1)
		asm volatile("membar.sys;" ::: "memory");
	else if (FK == 2)
		asm volatile("membar.gl;" ::: "memory");
	else if (FK == 3)
		asm volatile("fence.acq_rel.gpu;" ::: "memory");
	else if (FK == 4)
		asm volatile("fence.acq_rel.sys;" ::: "memory");
}
// the fence alone, executed by every lane (ALL = 1, one warp instruction when
// converged) or by lane 0 only
template <int FK, int ALL> __device__ __forceinline__ void fence_alone SIG
{
	const Ids i = ids();
	if (!ALL && i.lane != 0)
		return;
#pragma unroll 1
	for (uint32_t r = 0; r < R; r++)
		fence_k<FK>();
}
// st_lane32 / st_lane1 of step 0 with the fence kind FK
template <int FK> __device__ __forceinline__ void st32_fk SIG
{
	const Ids i = ids();
#pragma unroll 1
	for (uint32_t r = 0; r < R; r++) {
		st_u64(buf + ((r * i.nthreads + i.tid) & mask), i.tid + r + 1);
		fence_k<FK>();
	}
}
template <int FK> __device__ __forceinline__ void st1_fk SIG
{
	const Ids i = ids();
	if (i.lane != 0)
		return;
#pragma unroll 1
	for (uint32_t r = 0; r < R; r++) {
		st_u64(buf + ((r * i.nthreads + i.tid) & mask), i.tid + r + 1);
		fence_k<FK>();
	}
}

#define INST_T(kname, call)                                                    \
	extern "C" __global__ void kname SIG                                   \
	{                                                                      \
		call(buf, mask, R, k, aux, mode);                              \
	}
INST_T(k_slot_e, (scratch<0, 0>))
INST_T(k_slot_ex, (scratch<0, 1>))
INST_T(k_slot_excg, (scratch<0, 2>))
INST_T(k_tid_e, (scratch<1, 0>))
INST_T(k_tid_ex, (scratch<1, 1>))
INST_T(k_tidw_e, (scratch<2, 0>))
INST_T(k_slot_cx, (scratch_claim<1>))
INST_T(k_slot_cxr, (scratch_claim<0>))
INST_T(k_fal32_fk1, (fence_alone<1, 1>))
INST_T(k_fal32_fk2, (fence_alone<2, 1>))
INST_T(k_fal32_fk3, (fence_alone<3, 1>))
INST_T(k_fal32_fk4, (fence_alone<4, 1>))
INST_T(k_fal1_fk1, (fence_alone<1, 0>))
INST_T(k_fal1_fk2, (fence_alone<2, 0>))
INST_T(k_fal1_fk3, (fence_alone<3, 0>))
INST_T(k_fal1_fk4, (fence_alone<4, 0>))
INST_T(k_st32_fk2, (st32_fk<2>))
INST_T(k_st32_fk3, (st32_fk<3>))
INST_T(k_st32_fk4, (st32_fk<4>))
INST_T(k_st1_fk2, (st1_fk<2>))

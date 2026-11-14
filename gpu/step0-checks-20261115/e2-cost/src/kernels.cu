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

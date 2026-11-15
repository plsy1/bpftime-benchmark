// c4-batch-fence cost kernels (step 0b, check C4): one system fence for many
// warps' records (batched publication).
//
// Built with nvcc -cubin for sm_87 and launched with cuLaunchKernel by bfc
// (src/bfc.cpp), the launch path of step 0's bench2.
//
// One record per warp per launch (R = 1), the shape of an exit probe. Warp w
// (global, logical: (blockIdx.x * blockDim.x + threadIdx.x) / 32) belongs to
// chunk c = w / G; the last chunk of a launch may be partial, so its size is
// E = min(G, warps - c * G). Layouts:
//   w32  one 32-B record per warp, written by lane 0 as 4 x u64 (step 0's
//        rec_lane0); records at rec + 4 w words
//   l8   one 8-B word per lane (256 B per warp, one coalesced store per
//        warp); records at rec + 32 w words
// Protocol (variant V, below): every warp writes its record, orders it with a
// gpu-scope release and increments its chunk's counter (device memory, the
// CPU never reads it); the warp whose increment completes the chunk acquires
// at gpu scope, issues ONE system-scope fence and stores the chunk's flag
// (st.relaxed.sys; registered memory, one 128-B line per chunk).
//
// Completion test without a modulo: the host zeroes the counters before each
// batch and passes epoch = 1, 2, ... to the launches of the batch, so the
// completing increment of launch `epoch` is the one that returns
// epoch * E - 1 (epoch * 32 E - 1 for ar32, where every lane increments).
//
// Every fence, store and atomic is inline PTX (checked in SASS by
// sass_check.py).
#include <cstdint>

#define FULL_MASK 0xffffffffu
#define FLAG_STRIDE 16ull // words: one 128-B line per chunk flag
#define CNT_STRIDE 4ull // words: one 32-B sector per chunk counter
// aux (device memory, validation mode only): [0] completers, [1] contributing
// increments, [2] sink
#define AUX_COMPLETERS 0
#define AUX_CONTRIB 1

enum Layout { L_W32 = 0, L_L8 = 1 };
enum Variant {
	V_REC = 0, // records only: no counter, no flag, no fence
	V_RF_SCGPU, // record, lane 0 fence.sc.gpu (= membar.gl = __threadfence)
	V_RF_ARGPU, // record, lane 0 fence.acq_rel.gpu
	V_RF_SCSYS, // record, lane 0 fence.sc.sys (= membar.sys; step 0's rec_lane0 f1)
	V_RF_ARSYS, // record, lane 0 fence.acq_rel.sys
	V_PW_SC, // per-warp publication: record, fence.sc.sys, st.relaxed.sys flag[w]
	V_PW_AR, // per-warp publication: record, fence.acq_rel.sys, st.relaxed.sys flag[w]
	V_NOFENCE, // batched, no fence anywhere (unsafe lower bound)
	V_NOSYS, // batched, gpu-scope release/acquire, no system fence (control)
	V_SYSONLY, // batched, contributors without a fence, completer as AR_SC (control)
	V_TF_SC, // contributors fence.sc.gpu; completer fence.acq_rel.gpu + fence.sc.sys
	V_AR_SC, // contributors fence.acq_rel.gpu; completer fence.acq_rel.gpu + fence.sc.sys
	V_REL_SC, // contributors atom.release.gpu; completer fence.acq_rel.gpu + fence.sc.sys
	V_AR_AR, // contributors fence.acq_rel.gpu; completer fence.acq_rel.sys only
	V_REL_AR, // contributors atom.release.gpu; completer fence.acq_rel.sys only
	V_AR32_SC, // l8: every lane fence.acq_rel.gpu + its own increment; completer as AR_SC
	V_AR32_AR, // l8: every lane fence.acq_rel.gpu + its own increment; completer as AR_AR
	// the completer's acquire and its system release in separate instructions
	V_AR_AA, // contributors fence.acq_rel.gpu; completer fence.acq_rel.gpu + fence.acq_rel.sys
	V_RMW_AR, // every warp atom.acq_rel.gpu; completer fence.acq_rel.sys
	V_AR32_AA, // l8: every lane as AR32_*; completer as AR_AA
};

struct Params {
	uint64_t *rec; // registered: records
	uint64_t *cnt; // device (registered for the creg items): counters
	uint64_t *flags; // registered: flags
	uint64_t *aux; // device: validation counts
	uint64_t magic; // c = (w * magic) >> 40 == w / G (host-checked)
	uint32_t G; // warps per chunk (= per system fence)
	uint32_t epoch; // launch number within the batch (1-based)
	uint32_t mode; // bit 0: validation (count completers and increments)
	uint32_t pad;
};

__device__ __forceinline__ void st_u64(uint64_t *p, uint64_t v)
{
	asm volatile("st.global.u64 [%0], %1;" ::"l"(p), "l"(v) : "memory");
}
__device__ __forceinline__ void st_relaxed_sys(uint64_t *p, uint64_t v)
{
	asm volatile("st.relaxed.sys.global.u64 [%0], %1;" ::"l"(p), "l"(v)
		     : "memory");
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
__device__ __forceinline__ uint64_t atom_release_gpu_add(uint64_t *p, uint64_t v)
{
	uint64_t r;
	asm volatile("atom.release.gpu.global.add.u64 %0, [%1], %2;"
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
__device__ __forceinline__ uint64_t ld_volatile(const uint64_t *p)
{
	uint64_t r;
	asm volatile("ld.volatile.global.u64 %0, [%1];"
		     : "=l"(r)
		     : "l"(p)
		     : "memory");
	return r;
}
__device__ __forceinline__ uint64_t gtimer()
{
	uint64_t t;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	return t;
}
__host__ __device__ __forceinline__ uint64_t rec_value(uint32_t epoch, uint32_t w,
							uint32_t lane, uint32_t j)
{
	return ((uint64_t)epoch << 40) | ((uint64_t)w << 8) | (lane << 2) | j;
}

// ---- baseline ----------------------------------------------------------------
extern "C" __global__ void k_empty(Params p)
{
}

// ---- timing gate (step 0's bench2 k_hold, unchanged) -------------------------
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

// ---- fences alone (lane 0 of every warp, no store) ---------------------------
extern "C" __global__ void k_fonly_scgpu(Params p)
{
	if ((threadIdx.x & 31) == 0)
		fence_sc_gpu();
}
extern "C" __global__ void k_fonly_argpu(Params p)
{
	if ((threadIdx.x & 31) == 0)
		fence_acq_rel_gpu();
}
extern "C" __global__ void k_fonly_scsys(Params p)
{
	if ((threadIdx.x & 31) == 0)
		fence_sc_sys();
}
extern "C" __global__ void k_fonly_arsys(Params p)
{
	if ((threadIdx.x & 31) == 0)
		fence_acq_rel_sys();
}

// ---- records and publication ---------------------------------------------------
template <int L, int V> __device__ __forceinline__ void body(const Params &p)
{
	const uint32_t tid = blockIdx.x * blockDim.x + threadIdx.x;
	const uint32_t w = tid >> 5, lane = threadIdx.x & 31u;
	const uint32_t nwarps = (gridDim.x * blockDim.x) >> 5;
	// 1. the record
	if (L == L_W32) {
		if (lane != 0)
			return;
		uint64_t *r = p.rec + (uint64_t)w * 4;
		st_u64(r + 0, rec_value(p.epoch, w, 0, 0));
		st_u64(r + 1, rec_value(p.epoch, w, 0, 1));
		st_u64(r + 2, rec_value(p.epoch, w, 0, 2));
		st_u64(r + 3, rec_value(p.epoch, w, 0, 3));
	} else {
		st_u64(p.rec + (uint64_t)w * 32 + lane, rec_value(p.epoch, w, lane, 0));
	}
	if (V == V_REC)
		return;
	// l8: the other lanes' stores reach lane 0's release through
	// bar.warp.sync (converged: BRA.CONV, no barrier instruction). ar32_* do
	// not need it: every lane releases its own store.
	if (L == L_L8 && V != V_AR32_SC && V != V_AR32_AR && V != V_AR32_AA) {
		__syncwarp(FULL_MASK);
		if (lane != 0)
			return;
	}
	// 2a. a fence after the record, nothing else
	if (V == V_RF_SCGPU || V == V_RF_ARGPU || V == V_RF_SCSYS ||
	    V == V_RF_ARSYS) {
		if (V == V_RF_SCGPU)
			fence_sc_gpu();
		else if (V == V_RF_ARGPU)
			fence_acq_rel_gpu();
		else if (V == V_RF_SCSYS)
			fence_sc_sys();
		else
			fence_acq_rel_sys();
		return;
	}
	// 2b. per-warp publication (step 0's protocol A, plus its flag)
	if (V == V_PW_SC || V == V_PW_AR) {
		if (V == V_PW_SC)
			fence_sc_sys();
		else
			fence_acq_rel_sys();
		st_relaxed_sys(p.flags + (uint64_t)w * FLAG_STRIDE,
			       ((uint64_t)p.epoch << 32) | w);
		if (p.mode & 1) {
			atomicAdd((unsigned long long *)p.aux + AUX_COMPLETERS, 1ull);
			atomicAdd((unsigned long long *)p.aux + AUX_CONTRIB, 1ull);
		}
		return;
	}
	// 3. batched publication
	const uint32_t c = (uint32_t)(((uint64_t)w * p.magic) >> 40); // w / G
	const uint32_t E = min(p.G, nwarps - c * p.G); // last chunk may be partial
	uint64_t *ctr = p.cnt + (uint64_t)c * CNT_STRIDE;
	uint64_t old;
	uint64_t target = (uint64_t)p.epoch * E;
	if (V == V_AR32_SC || V == V_AR32_AR || V == V_AR32_AA) {
		target *= 32;
		fence_acq_rel_gpu();
		old = atom_relaxed_gpu_add(ctr, 1);
	} else if (V == V_NOFENCE || V == V_SYSONLY) {
		old = atom_relaxed_gpu_add(ctr, 1);
	} else if (V == V_TF_SC) {
		fence_sc_gpu();
		old = atom_relaxed_gpu_add(ctr, 1);
	} else if (V == V_NOSYS || V == V_AR_SC || V == V_AR_AR || V == V_AR_AA) {
		fence_acq_rel_gpu();
		old = atom_relaxed_gpu_add(ctr, 1);
	} else if (V == V_RMW_AR) {
		old = atom_acq_rel_gpu_add(ctr, 1); // release and acquire in one RMW
	} else { // V_REL_SC, V_REL_AR
		old = atom_release_gpu_add(ctr, 1);
	}
	if (p.mode & 1)
		atomicAdd((unsigned long long *)p.aux + AUX_CONTRIB, 1ull);
	if (old + 1 != target)
		return;
	// the completer
	if (V == V_NOFENCE) {
	} else if (V == V_NOSYS) {
		fence_acq_rel_gpu();
	} else if (V == V_AR_AR || V == V_REL_AR || V == V_AR32_AR) {
		fence_acq_rel_sys(); // acquire (gpu-scope chain) and sys release in one
	} else if (V == V_RMW_AR) {
		fence_acq_rel_sys(); // the acquire was the RMW itself
	} else if (V == V_AR_AA || V == V_AR32_AA) {
		fence_acq_rel_gpu(); // closes the acquire pattern
		fence_acq_rel_sys(); // opens the system release pattern
	} else { // SYSONLY, TF_SC, AR_SC, REL_SC, AR32_SC
		fence_acq_rel_gpu();
		fence_sc_sys();
	}
	st_relaxed_sys(p.flags + (uint64_t)c * FLAG_STRIDE,
		       ((uint64_t)p.epoch << 32) | w);
	if (p.mode & 1)
		atomicAdd((unsigned long long *)p.aux + AUX_COMPLETERS, 1ull);
}

#define INST(L, LN, V, VN)                                                     \
	extern "C" __global__ void k_##LN##_##VN(Params p)                     \
	{                                                                      \
		body<L, V>(p);                                                 \
	}
#define INST_BOTH(V, VN)                                                       \
	INST(L_W32, w32, V, VN)                                                \
	INST(L_L8, l8, V, VN)

INST_BOTH(V_REC, rec)
INST_BOTH(V_RF_SCGPU, rf_scgpu)
INST_BOTH(V_RF_ARGPU, rf_argpu)
INST_BOTH(V_RF_SCSYS, rf_scsys)
INST_BOTH(V_RF_ARSYS, rf_arsys)
INST_BOTH(V_PW_SC, pw_sc)
INST_BOTH(V_PW_AR, pw_ar)
INST_BOTH(V_NOFENCE, nofence)
INST_BOTH(V_NOSYS, nosys)
INST_BOTH(V_SYSONLY, sysonly)
INST_BOTH(V_TF_SC, tf_sc)
INST_BOTH(V_AR_SC, ar_sc)
INST_BOTH(V_REL_SC, rel_sc)
INST_BOTH(V_AR_AR, ar_ar)
INST_BOTH(V_REL_AR, rel_ar)
INST(L_L8, l8, V_AR32_SC, ar32_sc)
INST(L_L8, l8, V_AR32_AR, ar32_ar)
INST_BOTH(V_AR_AA, ar_aa)
INST_BOTH(V_RMW_AR, rmw_ar)
INST(L_L8, l8, V_AR32_AA, ar32_aa)

// kernels.cu: check C2 (step 0b). Cost kernels (c_*, timed by c2bench), correctness kernels
// (x_*, run by c2chk) and the __activemask() reproducer (x_mask). One module; the pool
// descriptor is the module's __constant__ g_pd (as bpftime's map_info is).
// Built with nvcc -cubin for sm_87 (native) and as PTX for the driver-JIT path (C2_JIT_SUBSET).
#include "c2.cuh"

// ---- timing gate, copied from step0/e2-cost/src/kernels.cu (k_hold) ------------------------------
#define HOLD_ITER_CAP (1u << 24)
__device__ __forceinline__ u64 ld_volatile(const u64 *p)
{
	u64 r;
	asm volatile("ld.volatile.global.u64 %0, [%1];" : "=l"(r) : "l"(p) : "memory");
	return r;
}
extern "C" __global__ void k_hold(u64 *hostw, u64 cap_ns)
{
	if (threadIdx.x != 0)
		return;
	const u64 t0 = r_gtimer();
#pragma unroll 1
	for (u32 it = 0; it < HOLD_ITER_CAP; it++) {
		if (ld_volatile(hostw) != 0)
			return;
		if (r_gtimer() - t0 > cap_ns)
			break;
		__nanosleep(256);
	}
	asm volatile("st.global.u64 [%0], %1;" ::"l"(hostw + 1), "l"(ld_volatile(hostw + 1) + 1) : "memory");
}

#ifndef C2_JIT_SUBSET
// ---- cost kernels: one entry helper call, one exit helper call per thread ----------------------
// Signature (u64 *aux, u32 mode) for every timed kernel; mode bit 0 = validation counting.
#define CB __global__ void __launch_bounds__(512, 3)
extern "C" CB c_empty(u64 *aux, u32 mode)
{
}
extern "C" CB c_probe(u64 *aux, u32 mode)
{
	St st = c2_entry_probe(mode);
	c2_exit_probe(st, mode, 0);
}
extern "C" CB c_wid(u64 *aux, u32 mode)
{
	St st = c2_entry_wid(mode);
	c2_exit_wid(st, mode, 0);
}
template <int P, int ORD, int X> __device__ __forceinline__ void cost_body(u32 mode)
{
	St st = c2_entry<P, ORD, X, 0>(mode);
	c2_exit<P, ORD, X, 0>(st, mode, 0);
}
#define CK_COST(name, P, ORD, X)                                                                  \
	extern "C" CB c_##name(u64 *aux, u32 mode)                                                \
	{                                                                                          \
		cost_body<P, ORD, X>(mode);                                                        \
	}
#define CK_COSTW(pn, P)                                                                            \
	CK_COST(pn##_o_lane, P, 1, C2_X_LANE)                                                      \
	CK_COST(pn##_r_lane, P, 0, C2_X_LANE)                                                      \
	CK_COST(pn##_oc_lane, P, 1, C2_X_CMASK)                                                    \
	CK_COST(pn##_o_agg, P, 1, C2_X_AGG)                                                        \
	CK_COST(pn##_r_agg, P, 0, C2_X_AGG)
CK_COSTW(bmsm, C2_BMSM)
CK_COSTW(bmglob, C2_BMGLOB)
CK_COSTW(stksm, C2_STKSM)
CK_COSTW(stkglob, C2_STKGLOB)
CK_COST(lnsm_o_lane, C2_LNSM, 1, C2_X_LANE)
CK_COST(lnsm_r_lane, C2_LNSM, 0, C2_X_LANE)
CK_COST(lnglob_o_lane, C2_LNGLOB, 1, C2_X_LANE)
CK_COST(lnglob_r_lane, C2_LNGLOB, 0, C2_X_LANE)
CK_COST(lfsm_o_lane, C2_LFSM, 1, C2_X_LANE)
CK_COST(lfsm_r_lane, C2_LFSM, 0, C2_X_LANE)
CK_COST(lfglob_o_lane, C2_LFGLOB, 1, C2_X_LANE)
CK_COST(lfglob_r_lane, C2_LFGLOB, 0, C2_X_LANE)
#endif

// ---- correctness kernels -----------------------------------------------------------------------
#define XIN(i) (((const u32 *)a.in)[((u64)(i) + a.off) & (a.nin - 1)])
#define XE() c2_entry<P, ORD, X, 1>(0)
#define XX(st, step) c2_exit<P, ORD, X, 1>((st), 0, (step))
#define XCHK(st)                                                                                   \
	do {                                                                                       \
		c2_check<P>(st);                                                                   \
		nck++;                                                                             \
	} while (0)

__device__ __forceinline__ void x_start(const XArgs &a)
{
	if (blockIdx.x == 0 && threadIdx.x == 0)
		cnt(CT_LAUNCHES);
	if (a.tl && (threadIdx.x & 31) == 0)
		atomicMin((u64 *)a.tl + 2 * a.launch_id, r_gtimer());
}
__device__ __forceinline__ void x_fin(const XArgs &a, u32 nck)
{
	if (nck)
		cnt(CT_CHECKS, nck);
	if (a.tl)
		atomicMax((u64 *)a.tl + 2 * a.launch_id + 1, r_gtimer());
}

// Divergent exits (patterns of step0/e7-warp): every lane checks its claim at every step and
// leaves through one of several exit sites at a data-dependent time. Every case except split,
// churn and entrydiv claims as its first statement or right after a full __syncwarp() (needed
// by C2_X_CMASK).
template <int P, int ORD, int X> __device__ __forceinline__ void div_body(const XArgs &a)
{
	const u32 gtid = blockIdx.x * blockDim.x + threadIdx.x;
	const u32 lane = r_laneid();
	u32 nck = 0;
	switch (a.kase) {
	case K_EARLY: { // about 1 lane in 16 leaves per iteration
		St st = XE();
		x_start(a);
#pragma unroll 1
		for (u32 it = 0; it < a.iters; it++) {
			const u32 v = XIN(it * a.nthreads + gtid);
			if ((v & 15) == 0) {
				XCHK(st);
				XX(st, it * 4 + 0);
				x_fin(a, nck);
				return;
			}
			if (v & 0x40)
				__nanosleep((v >> 8) & 511);
			XCHK(st);
		}
		XCHK(st);
		XX(st, 0xffff);
		break;
	}
	case K_LOWLANES: { // lane 0 and the lowest 1-31 lanes leave right after entry
		St st = XE();
		x_start(a);
		const u32 cut = 1 + XIN(gtid & ~31u) % 31;
		if (lane < cut) {
			XCHK(st);
			XX(st, 0xfffe);
			x_fin(a, nck);
			return;
		}
#pragma unroll 1
		for (u32 it = 0; it < a.iters; it++) {
			const u32 v = XIN(it * a.nthreads + gtid);
			if ((v & 31) == 0) {
				XCHK(st);
				XX(st, it * 4 + 0);
				x_fin(a, nck);
				return;
			}
			if (v & 0x40)
				__nanosleep((v >> 8) & 511);
			else if (v & 0x80)
				XCHK(st);
			XCHK(st);
		}
		XCHK(st);
		XX(st, 0xffff);
		break;
	}
	case K_NESTED: { // exit sites inside nested divergent branches
		St st = XE();
		x_start(a);
#pragma unroll 1
		for (u32 it = 0; it < a.iters; it++) {
			const u32 v = XIN(it * a.nthreads + gtid);
			if (v & 0x100) {
				if (v & 0x200) {
					if ((v & 31) == 0) {
						XCHK(st);
						XX(st, it * 4 + 0);
						x_fin(a, nck);
						return;
					}
					XCHK(st);
				} else {
					if ((v & 0x400) && (v & 0xf000) == 0) {
						XCHK(st);
						XX(st, it * 4 + 1);
						x_fin(a, nck);
						return;
					}
					__nanosleep((v >> 16) & 255);
				}
			} else if (v & 0x800) {
				if ((v & 0x1f0000) == 0) {
					XCHK(st);
					XX(st, it * 4 + 2);
					x_fin(a, nck);
					return;
				}
				XCHK(st);
			}
		}
		XCHK(st);
		XX(st, 0xffff);
		break;
	}
	case K_LOOPS: { // checks inside lane-dependent loops with data-dependent breaks
		St st = XE();
		x_start(a);
#pragma unroll 1
		for (u32 it = 0; it < a.iters; it++) {
			const u32 v = XIN(it * a.nthreads + gtid);
			if ((v & 63) == 0) {
				XCHK(st);
				XX(st, it * 4 + 0);
				x_fin(a, nck);
				return;
			}
			const u32 trip = (v >> 8) % 6 + (lane & 3);
#pragma unroll 1
			for (u32 j = 0; j < trip; j++) {
				XCHK(st);
				if (((v >> (16 + j)) & 3) == 0)
					break;
			}
		}
		XCHK(st);
		XX(st, 0xffff);
		break;
	}
	case K_SPLIT: { // the claim runs under divergence: up to 3 groups per warp, apart in time
		x_start(a);
		const u32 g = XIN(gtid) % 3;
		St st;
		st.s = C2_NONE;
		st.gen = 0;
		st.tag = 0;
		if (g == 0)
			st = XE();
		__syncwarp();
		if (g == 1)
			st = XE();
		__syncwarp();
		if (g == 2)
			st = XE();
#pragma unroll 1
		for (u32 it = 0; it < a.iters; it++) {
			const u32 v = XIN((it + 1) * a.nthreads + gtid);
			if ((v & 15) == 0) {
				XCHK(st);
				XX(st, it * 4 + 0);
				x_fin(a, nck);
				return;
			}
			if (v & 0x40)
				__nanosleep((v >> 8) & 511);
			XCHK(st);
		}
		XCHK(st);
		XX(st, 0xffff);
		break;
	}
	case K_ENTRYDIV: { // lane-dependent sleep, then one entry call with no barrier before it
		x_start(a);
		__nanosleep(XIN(gtid) & 2047);
		St st = XE();
#pragma unroll 1
		for (u32 it = 0; it < a.iters; it++) {
			const u32 v = XIN((it + 1) * a.nthreads + gtid);
			if ((v & 15) == 0) {
				XCHK(st);
				XX(st, it * 4 + 0);
				x_fin(a, nck);
				return;
			}
			if (v & 0x40)
				__nanosleep((v >> 8) & 511);
			XCHK(st);
		}
		XCHK(st);
		XX(st, 0xffff);
		break;
	}
	case K_CHURN:       // many claims per thread; lanes leave each claim at different steps
	case K_CHURNSYNC: { // the same with __syncwarp() before each claim (every lane is there)
		x_start(a);
		const bool sync = a.kase == K_CHURNSYNC;
#pragma unroll 1
		for (u32 r = 0; r < a.rounds; r++) {
			if (sync)
				__syncwarp();
			St st = XE();
			const u32 v = XIN(r * a.nthreads + gtid);
			const u32 xp = v % (a.J + 1);
			bool done = false;
#pragma unroll 1
			for (u32 j = 0; j < a.J; j++) {
				if (!done) {
					XCHK(st);
					if (j == xp) {
						XX(st, r * 64 + j);
						done = true;
					} else if (v & (1u << (8 + j % 16))) {
						__nanosleep((v >> 24) * 4);
					}
				}
			}
			if (!done) {
				XCHK(st);
				XX(st, r * 64 + 63);
			}
		}
		break;
	}
	default:
		break;
	}
	x_fin(a, nck);
}

// Full occupancy, proven co-resident: every warp claims (first statement), then waits at a capped
// all-warp barrier (step0/e1-ids case a); its lanes then leave in four groups.
template <int P, int ORD, int X> __device__ __forceinline__ void full_body(const XArgs &a)
{
	const u32 lane = r_laneid();
	u32 nck = 0;
	St st = XE();
	x_start(a);
	XCHK(st);
	if ((threadIdx.x & 31) == 0) {
		u32 *arr = (u32 *)a.arrive;
		atomicAdd(arr, 1u);
		const u64 t0 = r_gtimer();
		u32 it = 0;
		bool capped = false;
#pragma unroll 1
		while (*(volatile u32 *)arr < a.total_warps) {
			if (++it >= a.bar_cap_iters || r_gtimer() - t0 > a.bar_cap_ns) {
				capped = true;
				break;
			}
		}
		if (capped)
			cnt(CT_BAR_CAP);
	}
	__syncwarp();
	XCHK(st);
#pragma unroll 1
	for (u32 g = 0; g < 4; g++) {
		if ((lane & 3) == g) {
			XCHK(st);
			XX(st, g);
		}
		__nanosleep(500 * (g + 1));
		if ((lane & 3) > g)
			XCHK(st);
	}
	x_fin(a, nck);
}

// Holds for milliseconds (time-slicing from a second process): lane groups leave at different
// times; lane 0 counts time steps > gap_ns; (%smid, %warpid) changes are counted (information).
template <int P, int ORD, int X> __device__ __forceinline__ void hold_body(const XArgs &a)
{
	const u32 lane = r_laneid();
	u32 nck = 0;
	St st = XE();
	x_start(a);
	const u32 sm0 = r_smid(), w0 = r_warpid();
	const u64 dur = a.hold_ns + (lane & 3) * a.hold_step_ns +
			(a.hold_jit_ns ? (u64)hash32(logical_warp()) % (a.hold_jit_ns + 1) : 0);
	const u64 t0 = r_gtimer();
	u64 prev = t0, gmax = 0;
	u32 gaps = 0, idch = 0, it = 0;
	const u32 cap = 1u << 24;
#pragma unroll 1
	for (; it < cap; it++) {
		const u64 now = r_gtimer();
		if (now - prev > a.gap_ns) {
			gaps++;
			if (now - prev > gmax)
				gmax = now - prev;
		}
		prev = now;
		if (now - t0 >= dur)
			break;
		if (r_smid() != sm0 || r_warpid() != w0)
			idch++;
		if (it % a.check_every == 0)
			XCHK(st);
		__nanosleep(a.sleep_ns);
	}
	if (it == cap)
		cnt(CT_HOLD_CAP);
	XCHK(st);
	XX(st, lane & 3);
	if (idch)
		cnt(CT_ID_CHANGE, idch);
	if ((threadIdx.x & 31) == 0) {
		if (gaps)
			cnt(CT_GAPS, gaps);
		if (gmax)
			atomicMax(PD_CTR + (u64)CT_GAP_MAX * C2_CTR_STRIDE, gmax);
	}
	atomicMin(PD_CTR + (u64)CT_HOLD_NS_MIN * C2_CTR_STRIDE, prev - t0);
	x_fin(a, nck);
}

// ---- the __activemask() reproducer ---------------------------------------------------------------
// The opportunistic pattern alone: mask = __activemask(), the lowest lane works, then every lane
// reads the leader's mask with __shfl_sync(mask, mask, leader). A lane whose leader's mask differs
// from its own (or does not contain it) means the lanes of one __shfl_sync did not agree on the
// mask, which the CUDA guide's warp __sync constraints (5.4.6.6) require.
__device__ u64 g_mwork[64];
template <int LOOP> __device__ __forceinline__ u32 mask_body(u32 v)
{
	const u32 mask = __activemask();
	const u32 lane = r_laneid();
	const u32 leader = __ffs(mask) - 1;
	u32 s = 0;
	if (lane == leader) {
		if (LOOP) {
#pragma unroll 1
			for (u32 i = 0; i < (v & 7) + 1; i++)
				s += (u32)atomicAdd(&g_mwork[(v + i) & 63], 1ull);
		} else {
			s += (u32)atomicAdd(&g_mwork[v & 63], 1ull);
		}
	}
	s = __shfl_sync(mask, s, leader);
	const u32 lm = __shfl_sync(mask, mask, leader);
	if (lm != mask)
		cnt(CT_MASK_DISAGREE);
	if (!((lm >> lane) & 1))
		cnt(CT_MASK_NOTIN);
	cnt(CT_MASK_CALLS);
	return lm ^ s;
}
template <int LOOP> __device__ __noinline__ u32 mask_probe_ni(u32 v)
{
	return mask_body<LOOP>(v);
}
template <int SHAPE> __device__ __forceinline__ u32 mask_call(u32 v)
{
	if constexpr (SHAPE == 0)
		return mask_probe_ni<1>(v);
	else if constexpr (SHAPE == 1)
		return mask_probe_ni<0>(v);
	else
		return mask_body<1>(v);
}
// the churn structure of div_body's K_CHURN, with the helper in place of the claim
template <int SHAPE> __device__ __forceinline__ void mask_kernel_body(const XArgs &a)
{
	const u32 gtid = blockIdx.x * blockDim.x + threadIdx.x;
	u32 acc = 0;
	x_start(a);
#pragma unroll 1
	for (u32 r = 0; r < a.rounds; r++) {
		acc += mask_call<SHAPE>(r);
		const u32 v = XIN(r * a.nthreads + gtid);
		const u32 xp = v % (a.J + 1);
		bool done = false;
#pragma unroll 1
		for (u32 j = 0; j < a.J; j++) {
			if (!done) {
				if (j == xp) {
					acc += mask_call<SHAPE>(j);
					done = true;
				} else if (v & (1u << (8 + j % 16))) {
					__nanosleep((v >> 24) * 4);
				}
			}
		}
	}
	if (acc == 0x5a5a5a5au)
		cnt(CT_CHECKS);
}
extern "C" __global__ void __launch_bounds__(128, 12) x_mask_loop_ni(XArgs a)
{
	mask_kernel_body<0>(a);
}
extern "C" __global__ void __launch_bounds__(128, 12) x_mask_one_ni(XArgs a)
{
	mask_kernel_body<1>(a);
}
extern "C" __global__ void __launch_bounds__(128, 12) x_mask_loop_inl(XArgs a)
{
	mask_kernel_body<2>(a);
}

#define XK_DIV(name, P, ORD, X)                                                                   \
	extern "C" __global__ void __launch_bounds__(128, 12) x_div_##name(XArgs a)               \
	{                                                                                          \
		div_body<P, ORD, X>(a);                                                            \
	}
#define XK_FULL(name, P, ORD, X)                                                                  \
	extern "C" __global__ void __maxnreg__(40) x_full_##name(XArgs a)                         \
	{                                                                                          \
		full_body<P, ORD, X>(a);                                                           \
	}
#define XK_HOLD(name, P, ORD, X)                                                                  \
	extern "C" __global__ void __launch_bounds__(128, 12) x_hold_##name(XArgs a)              \
	{                                                                                          \
		hold_body<P, ORD, X>(a);                                                           \
	}
#define XK_ALL3(name, P, ORD, X)                                                                  \
	XK_DIV(name, P, ORD, X)                                                                    \
	XK_FULL(name, P, ORD, X)                                                                   \
	XK_HOLD(name, P, ORD, X)

#ifdef C2_JIT_SUBSET
// driver-JIT path: the divergent cases and the controls, three pools, and the reproducer
XK_DIV(bmsm_o_lane, C2_BMSM, 1, C2_X_LANE)
XK_DIV(bmsm_oc_lane, C2_BMSM, 1, C2_X_CMASK)
XK_DIV(bmsm_o_first, C2_BMSM, 1, C2_X_FIRST)
XK_DIV(bmsm_oc_first, C2_BMSM, 1, C2_X_CFIRST)
XK_DIV(stkglob_o_lane, C2_STKGLOB, 1, C2_X_LANE)
XK_DIV(stkglob_o_first, C2_STKGLOB, 1, C2_X_FIRST)
XK_DIV(lnsm_o_lane, C2_LNSM, 1, C2_X_LANE)
XK_DIV(lnsm_o_first, C2_LNSM, 1, C2_X_FIRST)
XK_DIV(lfsm_o_lane, C2_LFSM, 1, C2_X_LANE)
XK_DIV(lfsm_o_first, C2_LFSM, 1, C2_X_FIRST)
#else
#define XK_POOLW(pn, P)                                                                            \
	XK_ALL3(pn##_o_lane, P, 1, C2_X_LANE)                                                      \
	XK_ALL3(pn##_r_lane, P, 0, C2_X_LANE)                                                      \
	XK_ALL3(pn##_oc_lane, P, 1, C2_X_CMASK)                                                    \
	XK_DIV(pn##_o_agg, P, 1, C2_X_AGG)                                                         \
	XK_DIV(pn##_o_first, P, 1, C2_X_FIRST)                                                     \
	XK_DIV(pn##_oc_first, P, 1, C2_X_CFIRST)
XK_POOLW(bmsm, C2_BMSM)
XK_POOLW(bmglob, C2_BMGLOB)
XK_POOLW(stksm, C2_STKSM)
XK_POOLW(stkglob, C2_STKGLOB)
XK_DIV(stksmnv_o_lane, C2_STKSM_NV, 1, C2_X_LANE)
XK_DIV(stkglobnv_o_lane, C2_STKGLOB_NV, 1, C2_X_LANE)
XK_DIV(stksmnv_oc_lane, C2_STKSM_NV, 1, C2_X_CMASK)
XK_DIV(stkglobnv_oc_lane, C2_STKGLOB_NV, 1, C2_X_CMASK)
#define XK_POOLL(pn, P)                                                                            \
	XK_ALL3(pn##_o_lane, P, 1, C2_X_LANE)                                                      \
	XK_ALL3(pn##_r_lane, P, 0, C2_X_LANE)                                                      \
	XK_DIV(pn##_o_first, P, 1, C2_X_FIRST)
XK_POOLL(lnsm, C2_LNSM)
XK_POOLL(lnglob, C2_LNGLOB)
XK_POOLL(lfsm, C2_LFSM)
XK_POOLL(lfglob, C2_LFGLOB)
#endif

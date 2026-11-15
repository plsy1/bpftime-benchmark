// gate.cu: the timing gate of step0/e2-cost (src/kernels.cu, k_hold), copied
// unchanged so that part (c) times batches the same way. Launched before the
// first event of a timed batch: thread 0 polls a host-mapped word until the
// host has queued the whole batch and set it, so that the events time GPU
// execution and not the host's launch rate. Capped by globaltimer and an
// iteration count; a cap hit is counted in hostw[1] and marks the batch.
#include <stdint.h>

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
__device__ __forceinline__ uint64_t ld_volatile(const uint64_t *p)
{
	uint64_t r;
	asm volatile("ld.volatile.global.u64 %0, [%1];" : "=l"(r) : "l"(p) : "memory");
	return r;
}
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

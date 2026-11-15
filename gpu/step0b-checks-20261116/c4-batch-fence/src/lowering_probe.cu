// lowering_probe.cu: how each fence/atomic form used by C4 lowers to SASS on
// sm_87 with this nvcc (the table in README "The protocol"). Compiled by
// build.sh to build/lowering_probe.sass; never run.
#include <cstdint>
#define K(name, body)                                                          \
	extern "C" __global__ void name(uint64_t *p, uint64_t *q, uint64_t *o) \
	{                                                                      \
		if ((threadIdx.x & 31) == 0) {                                 \
			body                                                   \
		}                                                              \
	}
K(k_fsc_gpu, asm volatile("st.global.u64 [%0], 1; fence.sc.gpu;" ::"l"(p) : "memory");)
K(k_membar_gl, asm volatile("st.global.u64 [%0], 1; membar.gl;" ::"l"(p) : "memory");)
K(k_far_gpu, asm volatile("st.global.u64 [%0], 1; fence.acq_rel.gpu;" ::"l"(p) : "memory");)
K(k_far_sys, asm volatile("st.global.u64 [%0], 1; fence.acq_rel.sys;" ::"l"(p) : "memory");)
K(k_membar_sys, asm volatile("st.global.u64 [%0], 1; membar.sys;" ::"l"(p) : "memory");)
K(k_atom_rel_gpu, uint64_t r; asm volatile("st.global.u64 [%1], 1; atom.release.gpu.global.add.u64 %0, [%2], 1;" : "=l"(r) : "l"(p), "l"(q) : "memory"); o[0] = r;)
K(k_atom_acqrel_gpu, uint64_t r; asm volatile("st.global.u64 [%1], 1; atom.acq_rel.gpu.global.add.u64 %0, [%2], 1;" : "=l"(r) : "l"(p), "l"(q) : "memory"); o[0] = r;)
K(k_atom_rlx_gpu, uint64_t r; asm volatile("atom.relaxed.gpu.global.add.u64 %0, [%1], 1;" : "=l"(r) : "l"(q) : "memory"); o[0] = r;)
K(k_red_rel_gpu, asm volatile("st.global.u64 [%0], 1; red.release.gpu.global.add.u64 [%1], 1;" ::"l"(p), "l"(q) : "memory");)
K(k_st_rel_sys, asm volatile("st.release.sys.global.u64 [%0], 1;" ::"l"(p) : "memory");)
K(k_ld_acq_gpu, uint64_t r; asm volatile("ld.acquire.gpu.global.u64 %0, [%1];" : "=l"(r) : "l"(q) : "memory"); o[0] = r;)
K(k_ld_acq_sys, uint64_t r; asm volatile("ld.acquire.sys.global.u64 %0, [%1];" : "=l"(r) : "l"(q) : "memory"); o[0] = r;)
K(k_threadfence, *p = 1; __threadfence();)
K(k_threadfence_system, *p = 1; __threadfence_system();)
// the bi-directional alias proxy fence (PTX 9.7.15.4: fence.proxy requires
// sm_70; the .release/.acquire alias forms require sm_90, so they are not
// available on sm_87). README "Virtual aliases".
K(k_fproxy_alias, asm volatile("st.global.u64 [%0], 1; fence.proxy.alias;" ::"l"(p) : "memory");)

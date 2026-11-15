// probe.cu (C5, compile only, never run): what the documented CUDA C++ constructs of claims 4
// and 9 lower to on sm_87 with CUDA 13.2. Each kernel: every lane stores a payload word, the
// warp synchronizes, the leader publishes a flag. Compare with step 0's e4 protocol A
// (membar.sys; st.relaxed.sys), D1 (st.release.sys) and D2 (fence.acq_rel.sys; st.relaxed.sys).
#include <cooperative_groups.h>
#include <cuda/atomic>
namespace cg = cooperative_groups;
typedef unsigned long long u64;

// claim 4, form F: __syncwarp, then a libcu++ system-scope seq_cst fence, then a relaxed
// system-scope atomic store of the flag
extern "C" __global__ void pub_cxx_fence(u64 *payload, u64 *flag, u64 v)
{
	const unsigned lane = threadIdx.x & 31;
	payload[threadIdx.x + blockIdx.x * blockDim.x] = v + lane;
	__syncwarp();
	if (lane == 0) {
		cuda::atomic_thread_fence(cuda::memory_order_seq_cst, cuda::thread_scope_system);
		cuda::atomic_ref<u64, cuda::thread_scope_system> f(flag[blockIdx.x * (blockDim.x / 32) + threadIdx.x / 32]);
		f.store(v, cuda::memory_order_relaxed);
	}
}

// claim 4, form R: __syncwarp, then a system-scope release store of the flag (no separate fence)
extern "C" __global__ void pub_cxx_release(u64 *payload, u64 *flag, u64 v)
{
	const unsigned lane = threadIdx.x & 31;
	payload[threadIdx.x + blockIdx.x * blockDim.x] = v + lane;
	__syncwarp();
	if (lane == 0) {
		cuda::atomic_ref<u64, cuda::thread_scope_system> f(flag[blockIdx.x * (blockDim.x / 32) + threadIdx.x / 32]);
		f.store(v, cuda::memory_order_release);
	}
}

// claim 4, form T: __syncwarp, __threadfence_system(), relaxed system-scope atomic store
extern "C" __global__ void pub_threadfence_system(u64 *payload, u64 *flag, u64 v)
{
	const unsigned lane = threadIdx.x & 31;
	payload[threadIdx.x + blockIdx.x * blockDim.x] = v + lane;
	__syncwarp();
	if (lane == 0) {
		__threadfence_system();
		cuda::atomic_ref<u64, cuda::thread_scope_system> f(flag[blockIdx.x * (blockDim.x / 32) + threadIdx.x / 32]);
		f.store(v, cuda::memory_order_relaxed);
	}
}

// claim 9: warp-aggregated reservation, raw intrinsics (step 0 e7's reserve_ok) ...
extern "C" __device__ __noinline__ unsigned reserve_raw(unsigned *cursor)
{
	unsigned mask = __activemask();
	unsigned lane = threadIdx.x & 31;
	unsigned leader = __ffs(mask) - 1;
	unsigned n = __popc(mask);
	unsigned rank = __popc(mask & ((1u << lane) - 1));
	unsigned base = 0;
	if (lane == leader)
		base = atomicAdd(cursor, n);
	base = __shfl_sync(mask, base, leader);
	return base + rank;
}

// ... and the same with the documented cooperative-groups API (CPG 5.7.3.1.5 coalesced_group)
extern "C" __device__ __noinline__ unsigned reserve_cg(unsigned *cursor)
{
	cg::coalesced_group g = cg::coalesced_threads();
	unsigned base = 0;
	if (g.thread_rank() == 0)
		base = atomicAdd(cursor, (unsigned)g.num_threads());
	base = g.shfl(base, 0);
	return base + (unsigned)g.thread_rank();
}

extern "C" __global__ void use_reserve(unsigned *cursor, unsigned *out, const unsigned *in)
{
	const unsigned t = threadIdx.x + blockIdx.x * blockDim.x;
	if (in[t] & 1)
		return; // divergent early return
	out[t] = (in[t] & 2) ? reserve_raw(cursor) : reserve_cg(cursor + 1);
}

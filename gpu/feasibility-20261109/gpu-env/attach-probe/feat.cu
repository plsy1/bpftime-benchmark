// A few common CUDA idioms, to see which PTX ISA features nvcc 13.2 emits
// for sm_87/compute_75 (bpftime compiles patched modules as .version 8.1).
#include <cuda_fp16.h>
#include <cooperative_groups.h>
#include <cooperative_groups/reduce.h>
namespace cg = cooperative_groups;
struct S { float a, b, c, d; };
__global__ void k(const float4 *__restrict__ in, float *out, const S *s, __half *h, int n)
{
	int i = blockIdx.x * blockDim.x + threadIdx.x;
	__shared__ float sm[256];
	if (i < n) {
		float4 v = in[i];
		sm[threadIdx.x] = v.x + s[i].c; // partial use of 16B loads
		__syncthreads();
		auto g = cg::tiled_partition<32>(cg::this_thread_block());
		float r = cg::reduce(g, sm[threadIdx.x], cg::plus<float>());
		h[i] = __hadd(h[i], __float2half(r));
		out[i] = r + __shfl_xor_sync(0xffffffff, r, 1);
		atomicAdd(&out[0], 1.0f);
		__nanosleep(10);
	}
}

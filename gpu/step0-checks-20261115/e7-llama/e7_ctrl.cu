// step0 e7-llama controls: kernels whose exit-probe masks are known.
//  - e7ctrl_partial: blocks of 48 threads, no branch, one exit. Warp 0 has
//    32 lanes, warp 1 has 16 (lanes 16-31 do not exist): per launch, G calls
//    with 32 lanes and G calls with 16. A helper 520 that returned a constant
//    full mask would report 2G calls of 32 lanes and 64 threads per block.
//  - e7ctrl_diverge: blocks of 64 threads (two full warps). Odd lanes leave
//    through their own ret (inline PTX); even lanes first wait ~200 us
//    (globaltimer, nanosleep, iteration cap) and then leave through the
//    kernel's final ret. The PTX has two rets, the kretprobe pass puts a
//    probe call before each, and the wait keeps the two groups apart, so
//    per warp: two calls of 16 lanes, the odd one led by lane 1. A helper
//    that ignored divergence (full mask, or the mask at kernel entry) would
//    give one call of 32 per warp (lane 0 leads, the odd group adds nothing)
//    and still match the thread total.
//  - e7ctrl_merge: the same two exits without the wait. Not a pass/fail
//    control: it shows whether two exit groups that reach the probe close
//    together merge inside it. In plain CUDA they do when the called
//    function starts with YIELD and a global load (bpftime's probe switch):
//    scratchpad test, 2026-11-15.
// The output check confirms that odd lanes really left early (they add 1,
// even lanes add 2).
// usage: e7_ctrl [launches=10] [blocks=64]
#include <cstdio>
#include <cstdlib>
#include <cuda_runtime.h>

#define WAIT_NS 200000ULL
#define WAIT_CAP 100000

extern "C" __global__ void e7ctrl_partial(int *out)
{
	out[blockIdx.x * blockDim.x + threadIdx.x] += 1;
}

static __device__ __forceinline__ unsigned long long gtimer()
{
	unsigned long long t;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	return t;
}

extern "C" __global__ void e7ctrl_diverge(int *out, unsigned *cap_hits)
{
	int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (threadIdx.x & 1) {
		out[i] += 1;
		asm volatile("ret;"); // the odd lanes' own exit
	}
	unsigned long long t0 = gtimer();
	int n = 0;
	while (gtimer() - t0 < WAIT_NS && n < WAIT_CAP) {
		__nanosleep(1000);
		n++;
	}
	if (n >= WAIT_CAP)
		atomicAdd(cap_hits, 1u);
	out[i] += 2;
}

extern "C" __global__ void e7ctrl_merge(int *out)
{
	int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (threadIdx.x & 1) {
		out[i] += 1;
		asm volatile("ret;");
	}
	out[i] += 2;
}

static size_t check(const int *h, size_t n, int expect_even, int expect_odd)
{
	size_t bad = 0;
	for (size_t i = 0; i < n; i++)
		bad += h[i] != ((i & 1) ? expect_odd : expect_even);
	return bad;
}

int main(int argc, char **argv)
{
	int launches = argc > 1 ? atoi(argv[1]) : 10;
	int blocks = argc > 2 ? atoi(argv[2]) : 64;
	const int tp = 48, td = 64;
	size_t np = (size_t)blocks * tp, nd = (size_t)blocks * td;
	int *dp = nullptr, *dd = nullptr, *dm = nullptr;
	unsigned *caps = nullptr;
	if (cudaMalloc(&dp, np * sizeof(int)) != cudaSuccess ||
	    cudaMalloc(&dd, nd * sizeof(int)) != cudaSuccess ||
	    cudaMalloc(&dm, nd * sizeof(int)) != cudaSuccess ||
	    cudaMalloc(&caps, sizeof(unsigned)) != cudaSuccess ||
	    cudaMemset(dp, 0, np * sizeof(int)) != cudaSuccess ||
	    cudaMemset(dd, 0, nd * sizeof(int)) != cudaSuccess ||
	    cudaMemset(dm, 0, nd * sizeof(int)) != cudaSuccess ||
	    cudaMemset(caps, 0, sizeof(unsigned)) != cudaSuccess) {
		printf("e7_ctrl: cudaMalloc/cudaMemset failed\n");
		return 2;
	}
	for (int i = 0; i < launches; i++) {
		e7ctrl_partial<<<blocks, tp>>>(dp);
		e7ctrl_diverge<<<blocks, td>>>(dd, caps);
		e7ctrl_merge<<<blocks, td>>>(dm);
	}
	cudaError_t e = cudaDeviceSynchronize();
	int *hp = (int *)malloc(np * sizeof(int));
	int *hd = (int *)malloc(nd * sizeof(int));
	int *hm = (int *)malloc(nd * sizeof(int));
	unsigned hcaps = 0;
	cudaMemcpy(hp, dp, np * sizeof(int), cudaMemcpyDeviceToHost);
	cudaMemcpy(hd, dd, nd * sizeof(int), cudaMemcpyDeviceToHost);
	cudaMemcpy(hm, dm, nd * sizeof(int), cudaMemcpyDeviceToHost);
	cudaMemcpy(&hcaps, caps, sizeof(unsigned), cudaMemcpyDeviceToHost);
	size_t badp = check(hp, np, launches, launches);
	size_t badd = check(hd, nd, 2 * launches, launches);
	size_t badm = check(hm, nd, 2 * launches, launches);
	printf("e7_ctrl: launches=%d blocks=%d sync=%s bad_elements partial=%zu diverge=%zu merge=%zu wait_cap_hits=%u\n",
	       launches, blocks, cudaGetErrorString(e), badp, badd, badm, hcaps);
	printf("e7_ctrl: expected e7ctrl_partial: launches=%d calls_32=%d calls_16=%d threads=%zu\n",
	       launches, launches * blocks, launches * blocks,
	       (size_t)launches * np);
	printf("e7_ctrl: expected e7ctrl_diverge: launches=%d calls_16=%d threads=%zu\n",
	       launches, 4 * launches * blocks, (size_t)launches * nd);
	printf("e7_ctrl: e7ctrl_merge (reported): launches=%d threads=%zu, calls between %d (merged) and %d (separate)\n",
	       launches, (size_t)launches * nd, 2 * launches * blocks, 4 * launches * blocks);
	free(hp);
	free(hd);
	free(hm);
	cudaFree(dp);
	cudaFree(dd);
	cudaFree(dm);
	cudaFree(caps);
	return (e == cudaSuccess && badp == 0 && badd == 0 && badm == 0 && hcaps == 0) ? 0 : 1;
}

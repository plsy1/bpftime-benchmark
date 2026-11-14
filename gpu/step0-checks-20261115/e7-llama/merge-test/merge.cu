// Does activemask in a __noinline__ function, called from two divergent
// exit sites, report the caller's group (16 lanes) or a merged warp (32)?
#include <cstdio>
#include <cuda_runtime.h>
extern "C" __noinline__ __device__ unsigned long long probe_mask(unsigned *out, int i)
{
	unsigned m;
	asm volatile("activemask.b32 %0;" : "=r"(m));
	out[i] = m;
	return m;
}
extern "C" __noinline__ __device__ unsigned long long probe_mask2(unsigned *out, int i)
{
	unsigned m;
	asm volatile("activemask.b32 %0;" : "=r"(m));
	out[i] = m;
	return m;
}
// A: two sites, same function
extern "C" __global__ void kA(unsigned *out)
{
	int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (threadIdx.x & 1) {
		probe_mask(out, i);
		asm volatile("ret;");
	}
	probe_mask(out, i);
}
// B: two sites, even lanes sleep first (capped loop)
extern "C" __global__ void kB(unsigned *out, unsigned *caps)
{
	int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (threadIdx.x & 1) {
		probe_mask(out, i);
		asm volatile("ret;");
	}
	unsigned long long t0 = clock64();
	int n = 0;
	while (clock64() - t0 < 200000 && n < 100000) { __nanosleep(1000); n++; }
	if (n >= 100000) atomicAdd(caps, 1);
	probe_mask(out, i);
}
// C: two sites, different functions
extern "C" __global__ void kC(unsigned *out)
{
	int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (threadIdx.x & 1) {
		probe_mask2(out, i);
		asm volatile("ret;");
	}
	probe_mask(out, i);
}
static void report(const char *k, unsigned *d, int n)
{
	unsigned h[64];
	cudaMemcpy(h, d, n * sizeof(unsigned), cudaMemcpyDeviceToHost);
	int full = 0, odd = 0, even = 0, other = 0;
	for (int i = 0; i < n; i++) {
		if (h[i] == 0xffffffffu) full++;
		else if (h[i] == 0xaaaaaaaau) odd++;
		else if (h[i] == 0x55555555u) even++;
		else other++;
	}
	printf("%s: lanes reporting full=%d odd-group=%d even-group=%d other=%d (lane0=%08x lane1=%08x)\n", k, full, odd, even, other, h[0], h[1]);
}
int main()
{
	unsigned *d, *caps; cudaMalloc(&d, 64 * sizeof(unsigned)); cudaMalloc(&caps, 4); cudaMemset(caps, 0, 4);
	for (int r = 0; r < 3; r++) {
		cudaMemset(d, 0, 256); kA<<<1, 64>>>(d); cudaDeviceSynchronize(); report("kA same fn, two sites", d, 64);
		cudaMemset(d, 0, 256); kB<<<1, 64>>>(d, caps); cudaDeviceSynchronize(); report("kB even lanes sleep  ", d, 64);
		cudaMemset(d, 0, 256); kC<<<1, 64>>>(d); cudaDeviceSynchronize(); report("kC two functions     ", d, 64);
	}
	unsigned hc; cudaMemcpy(&hc, caps, 4, cudaMemcpyDeviceToHost);
	printf("sleep cap hits=%u err=%s\n", hc, cudaGetErrorString(cudaGetLastError()));
	return 0;
}

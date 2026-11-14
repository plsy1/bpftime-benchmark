// Do two divergent groups that enter the same __noinline__ function merge
// there when the function stalls (global load, nanosleep) before activemask?
#include <cstdio>
#include <cuda_runtime.h>
__device__ volatile int g_flag = 1;
extern "C" __noinline__ __device__ void f_ld(unsigned *out, int i)
{
	if (g_flag == 0) return;  // like bpftime's probe switch
	unsigned m; asm volatile("activemask.b32 %0;" : "=r"(m)); out[i] = m;
}
extern "C" __noinline__ __device__ void f_sleep(unsigned *out, int i)
{
	if (g_flag == 0) return;
	__nanosleep(2000);
	unsigned m; asm volatile("activemask.b32 %0;" : "=r"(m)); out[i] = m;
}
#define KERNEL(name, fn) \
extern "C" __global__ void name(unsigned *out) { \
	int i = blockIdx.x * blockDim.x + threadIdx.x; \
	if (threadIdx.x & 1) { fn(out, i); asm volatile("ret;"); } \
	fn(out, i); }
KERNEL(kLd, f_ld)
KERNEL(kSleep, f_sleep)
static void report(const char *k, unsigned *d, int n)
{
	unsigned *h = new unsigned[n];
	cudaMemcpy(h, d, n * sizeof(unsigned), cudaMemcpyDeviceToHost);
	int full = 0, odd = 0, even = 0, other = 0;
	for (int i = 0; i < n; i++) {
		if (h[i] == 0xffffffffu) full++; else if (h[i] == 0xaaaaaaaau) odd++;
		else if (h[i] == 0x55555555u) even++; else other++;
	}
	printf("%s: lanes full=%d odd-group=%d even-group=%d other=%d\n", k, full, odd, even, other);
	delete[] h;
}
int main()
{
	const int n = 64 * 64;
	unsigned *d; cudaMalloc(&d, n * sizeof(unsigned));
	for (int r = 0; r < 2; r++) {
		cudaMemset(d, 0, n * 4); kLd<<<64, 64>>>(d); cudaDeviceSynchronize(); report("kLd    (flag load)     ", d, n);
		cudaMemset(d, 0, n * 4); kSleep<<<64, 64>>>(d); cudaDeviceSynchronize(); report("kSleep (flag+nanosleep)", d, n);
	}
	printf("err=%s\n", cudaGetErrorString(cudaGetLastError()));
	return 0;
}

// What does the CUDA 13.2 host stub pass as `func` to cudaLaunchKernel?
// bpftime's replacement looks func up in symbol_address_to_fatbin, which is
// keyed by the host stub address given to __cudaRegisterFunction.
#include <cuda_runtime.h>
#include <stdio.h>
__global__ void k(int *p) { if (p) *p = 1; }
#ifndef __NV_LEGACY_LAUNCH
extern "C" cudaError_t __cudaGetKernel(cudaKernel_t *, const void *);
#endif
int main() {
	cudaFree(0);
#ifndef __NV_LEGACY_LAUNCH
	cudaKernel_t h = nullptr;
	cudaError_t e = __cudaGetKernel(&h, (const void *)k);
	printf("host stub addr=%p  __cudaGetKernel handle=%p (%s)  same=%d\n",
	       (const void *)k, (void *)h, cudaGetErrorString(e), (const void *)k == (const void *)h);
#else
	printf("legacy launch build: stub passes host addr %p to cudaLaunchKernel\n", (const void *)k);
#endif
	k<<<1, 1>>>(nullptr);
	printf("launch: %s\n", cudaGetErrorString(cudaDeviceSynchronize()));
	return 0;
}

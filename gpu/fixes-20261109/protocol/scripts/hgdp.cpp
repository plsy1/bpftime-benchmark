#include <cuda_runtime.h>
#include <cstdio>
#include <cstdlib>
int main() {
	void *p = aligned_alloc(4096, 1 << 20), *d = nullptr;
	cudaError_t e = cudaHostGetDevicePointer(&d, p, 0);
	printf("cudaHostGetDevicePointer on unregistered memory, fresh process: %d (%s) d=%p host=%p\n", (int)e, cudaGetErrorString(e), d, p);
}

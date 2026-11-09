// Old CUDAContext (runtime API) against the new one (driver API) on host memory nobody registered,
// in a process that never called cuInit/cudaSetDevice, as the failing unit tests' processes.
#include <cuda.h>
#include <cuda_runtime.h>
#include <cstdio>
#include <sys/mman.h>
int main(int argc, char **argv)
{
	void *mem = mmap(nullptr, 4 << 20, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (argc > 1) { // new: driver API, no cuInit
		CUdeviceptr d = 0;
		CUresult r = cuMemHostGetDevicePointer(&d, mem, 0);
		printf("driver, no cuInit: %d\n", (int)r);
		r = cuInit(0);
		printf("cuInit: %d\n", (int)r);
		r = cuMemHostGetDevicePointer(&d, mem, 0);
		printf("driver after cuInit, no context: %d\n", (int)r);
		return 0;
	}
	void *d = nullptr;
	cudaError_t e = cudaHostGetDevicePointer(&d, mem, 0);
	printf("runtime, unregistered: %d (%s) dev=%p\n", (int)e, cudaGetErrorString(e), d);
	return 0;
}

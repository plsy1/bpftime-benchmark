#include <cuda.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
int main() {
	cuInit(0); CUdevice d; cuDeviceGet(&d, 0); CUcontext c; cuDevicePrimaryCtxRetain(&c, d); cuCtxSetCurrent(c);
	size_t sz = 1 << 20; void *p = aligned_alloc(4096, sz);
	printf("register %d\n", (int)cuMemHostRegister(p, sz, CU_MEMHOSTREGISTER_DEVICEMAP | CU_MEMHOSTREGISTER_PORTABLE));
	CUmemorytype t; int n = 2000;
	auto s = std::chrono::steady_clock::now();
	for (int i = 0; i < n; i++) cuPointerGetAttribute(&t, CU_POINTER_ATTRIBUTE_MEMORY_TYPE, (CUdeviceptr)((char *)p + (i * 64) % sz));
	auto e = std::chrono::steady_clock::now();
	printf("cuPointerGetAttribute: %.2f us each (type %d)\n", std::chrono::duration<double, std::micro>(e - s).count() / n, (int)t);
}

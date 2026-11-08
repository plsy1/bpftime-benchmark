// How much memory the driver takes when the per-thread stack grows to the
// size a bpftime probe needs (16496 B, CU_LIMIT_STACK_SIZE after the patched
// launch in mini_pipeline).
#include <cuda.h>
#include <cstdio>
int main()
{
	CUdevice dev;
	CUcontext ctx;
	cuInit(0);
	cuDeviceGet(&dev, 0);
	cuDevicePrimaryCtxRetain(&ctx, dev);
	cuCtxSetCurrent(ctx);
	size_t sizes[] = { 1024, 16496, 1024, 16496, 1024, 16496 };
	for (size_t s : sizes) {
		size_t f0, t, f1;
		cuMemGetInfo(&f0, &t);
		CUresult r = cuCtxSetLimit(CU_LIMIT_STACK_SIZE, s);
		cuMemGetInfo(&f1, &t);
		printf("set stack %5zu -> %d: free %zu -> %zu MiB (took %lld MiB)\n", s,
		       (int)r, f0 >> 20, f1 >> 20,
		       ((long long)f0 - (long long)f1) / (1 << 20));
	}
	return 0;
}

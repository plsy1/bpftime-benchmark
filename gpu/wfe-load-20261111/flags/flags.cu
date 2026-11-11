// What do CUDA's wait-mode flags report, can another thread read them, and
// what does each mode do on this Orin? flags MODE
//   MODE: none (never set), auto, spin, yield, blocking, late-spin (set spin
//   after the context is in use)
// Prints: cudaGetDeviceFlags, cuCtxGetFlags (calling thread),
// cuDevicePrimaryCtxGetState flags read from a second thread, and the CPU
// seconds the calling thread spends in cudaDeviceSynchronize over 50 launches
// of a 20 ms kernel (RUSAGE_THREAD).
#include <cstdio>
#include <cstring>
#include <ctime>
#include <thread>
#include <sys/resource.h>
#include <cuda.h>
#include <cuda_runtime.h>

__global__ void spinKernel(unsigned long long ns)
{
	unsigned long long t0, t;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t0));
	do {
		asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	} while (t - t0 < ns);
}

static double thread_cpu()
{
	rusage ru;
	getrusage(RUSAGE_THREAD, &ru);
	return ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1e6 +
	       ru.ru_stime.tv_sec + ru.ru_stime.tv_usec / 1e6;
}

static void report(const char *when)
{
	unsigned int rt = 0, ctxf = 0, pf = 0;
	int active = -1;
	cudaGetDeviceFlags(&rt);
	CUresult r1 = cuCtxGetFlags(&ctxf);
	CUresult r2 = CUDA_ERROR_UNKNOWN;
	// another thread, with no current context, as bpftime's watcher would
	std::thread([&] {
		CUdevice dev;
		cuDeviceGet(&dev, 0);
		r2 = cuDevicePrimaryCtxGetState(dev, &pf, &active);
	}).join();
	printf("%-14s cudaGetDeviceFlags=0x%x cuCtxGetFlags=%s0x%x other-thread PrimaryCtxGetState=%s0x%x active=%d\n",
	       when, rt, r1 == CUDA_SUCCESS ? "" : "ERR ", ctxf,
	       r2 == CUDA_SUCCESS ? "" : "ERR ", pf, active);
}

int main(int argc, char **argv)
{
	const char *mode = argc > 1 ? argv[1] : "none";
	unsigned int f = 0;
	bool late = false;
	if (!strcmp(mode, "auto")) f = cudaDeviceScheduleAuto;
	else if (!strcmp(mode, "spin")) f = cudaDeviceScheduleSpin;
	else if (!strcmp(mode, "yield")) f = cudaDeviceScheduleYield;
	else if (!strcmp(mode, "blocking")) f = cudaDeviceScheduleBlockingSync;
	else if (!strcmp(mode, "late-spin")) late = true;
	cuInit(0);
	if (strcmp(mode, "none") && !late) {
		cudaError_t e = cudaSetDeviceFlags(f);
		printf("cudaSetDeviceFlags(0x%x) before init: %s\n", f, cudaGetErrorString(e));
	}
	cudaFree(nullptr); // initializes the primary context
	report("after init");
	if (late) {
		cudaError_t e = cudaSetDeviceFlags(cudaDeviceScheduleSpin);
		printf("cudaSetDeviceFlags(spin) after init: %s\n", cudaGetErrorString(e));
		report("after late set");
	}
	spinKernel<<<1, 1>>>(1000);
	cudaDeviceSynchronize();
	double c0 = thread_cpu();
	timespec a, b;
	clock_gettime(CLOCK_MONOTONIC, &a);
	for (int i = 0; i < 50; i++) {
		spinKernel<<<1, 1>>>(20ULL * 1000 * 1000);
		cudaDeviceSynchronize();
	}
	clock_gettime(CLOCK_MONOTONIC, &b);
	double wall = (b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) / 1e9;
	printf("%-14s sync thread CPU %.3f s over %.3f s wall (%.1f%%)\n", mode,
	       thread_cpu() - c0, wall, 100 * (thread_cpu() - c0) / wall);
	return 0;
}

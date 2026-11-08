// Capability probe for bpftime's CUDA attach prerequisites on this board.
// Checks, in the same way bpftime uses them:
//  1. cudaSetDeviceFlags(cudaDeviceMapHost) + cudaHostRegister(Mapped) on a
//     MAP_SHARED shm_open() mapping (bpftime_shm_internal.cpp:1406-1424)
//  2. cudaHostGetDevicePointer on it (bpf_attach_ctx_cuda.cpp:412)
//  3. GPU -> host request / host -> GPU response handshake through that
//     mapping, like the CUDA watcher thread protocol (flag1/flag2)
//  4. cuIpcGetMemHandle on cuMemAlloc memory (nv_gpu_shared_array_map.cpp:55,
//     nv_gpu_per_thread_array_map.cpp:74)
//  5. %globaltimer readable in a kernel (helper 502)
#include <cuda.h>
#include <cuda_runtime.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <chrono>

struct Comm {
	volatile int flag1; // GPU -> host: request pending
	volatile int flag2; // host -> GPU: response ready
	volatile unsigned long long req;
	volatile unsigned long long resp;
	volatile unsigned long long gtimer;
};

__global__ void handshake(Comm *c, int rounds)
{
	for (int i = 0; i < rounds; i++) {
		c->req = 1000 + i;
		c->flag2 = 0;
		__threadfence_system();
		c->flag1 = 1;
		__threadfence_system();
		long long spins = 0;
		while (c->flag2 == 0) {
			if (++spins > (1LL << 31)) {
				c->resp = 0xdeadULL;
				return;
			}
		}
		__threadfence_system();
	}
	unsigned long long t;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	c->gtimer = t;
	__threadfence_system();
}

int main()
{
	char name[64];
	snprintf(name, sizeof(name), "/gpuenv-captest-%d", (int)getpid());
	size_t sz = 20 << 20;
	int fd = shm_open(name, O_CREAT | O_RDWR, 0600);
	if (fd < 0) {
		perror("shm_open");
		return 1;
	}
	if (ftruncate(fd, sz) != 0) {
		perror("ftruncate");
		return 1;
	}
	void *base = mmap(nullptr, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	shm_unlink(name);
	if (base == MAP_FAILED) {
		perror("mmap");
		return 1;
	}
	for (size_t i = 0; i < sz; i += 4096)
		((volatile char *)base)[i] = 0;

	cudaError_t e = cudaSetDeviceFlags(cudaDeviceMapHost);
	printf("[1] cudaSetDeviceFlags(cudaDeviceMapHost): %s\n", cudaGetErrorString(e));
	int canMap = 0, integrated = 0, cm = 0, cn = 0, pageable = 0;
	cudaDeviceGetAttribute(&canMap, cudaDevAttrCanMapHostMemory, 0);
	cudaDeviceGetAttribute(&integrated, cudaDevAttrIntegrated, 0);
	cudaDeviceGetAttribute(&cm, cudaDevAttrComputeCapabilityMajor, 0);
	cudaDeviceGetAttribute(&cn, cudaDevAttrComputeCapabilityMinor, 0);
	cudaDeviceGetAttribute(&pageable, cudaDevAttrPageableMemoryAccess, 0);
	printf("    device: cc=%d.%d integrated=%d canMapHostMemory=%d pageableMemoryAccess=%d\n",
	       cm, cn, integrated, canMap, pageable);

	e = cudaHostRegister(base, sz, cudaHostRegisterMapped);
	printf("[1] cudaHostRegister(MAP_SHARED shm, 20MB, Mapped): %s\n", cudaGetErrorString(e));
	if (e != cudaSuccess)
		return 2;
	void *dptr = nullptr;
	e = cudaHostGetDevicePointer(&dptr, base, 0);
	printf("[2] cudaHostGetDevicePointer: %s host=%p dev=%p\n", cudaGetErrorString(e), base, dptr);
	if (e != cudaSuccess)
		return 3;

	Comm *hc = (Comm *)base;
	memset((void *)hc, 0, sizeof(Comm));
	const int rounds = 50;
	std::atomic<bool> stop{ false };
	std::atomic<int> served{ 0 };
	std::thread watcher([&]() {
		while (!stop.load()) {
			if (hc->flag1 == 1) {
				hc->flag1 = 0;
				hc->resp = hc->req * 2;
				std::atomic_thread_fence(std::memory_order_seq_cst);
				hc->flag2 = 1;
				std::atomic_thread_fence(std::memory_order_seq_cst);
				served++;
			}
		}
	});
	auto t0 = std::chrono::steady_clock::now();
	handshake<<<1, 1>>>((Comm *)dptr, rounds);
	e = cudaDeviceSynchronize();
	auto t1 = std::chrono::steady_clock::now();
	stop = true;
	watcher.join();
	double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
	printf("[3] GPU<->host handshake: sync=%s served=%d/%d last_resp=%llu (expect %d) %.3f ms total\n",
	       cudaGetErrorString(e), served.load(), rounds, hc->resp, (1000 + rounds - 1) * 2, ms);
	printf("[5] %%globaltimer from kernel: %llu (0 means not read)\n", hc->gtimer);

	CUdeviceptr dmem = 0;
	CUresult r = cuMemAlloc(&dmem, 1 << 20);
	const char *rn = nullptr;
	cuGetErrorName(r, &rn);
	printf("[4] cuMemAlloc: %s\n", rn ? rn : "?");
	CUipcMemHandle h;
	r = cuIpcGetMemHandle(&h, dmem);
	rn = nullptr;
	cuGetErrorName(r, &rn);
	printf("[4] cuIpcGetMemHandle(cuMemAlloc buffer): %d %s\n", (int)r, rn ? rn : "?");
	cudaHostUnregister(base);
	munmap(base, sz);
	return 0;
}

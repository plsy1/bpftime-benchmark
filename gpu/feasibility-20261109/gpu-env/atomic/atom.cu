// GPU atomics on cudaHostRegister'ed MAP_SHARED shm (cuda-counter's
// __atomic_add_fetch on a host-map value pointer returned by the host helper).
#include <cuda_runtime.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
__global__ void add(unsigned long long *p, int n) {
	for (int i = 0; i < n; i++) atomicAdd(p, 1ULL);          // device scope
	for (int i = 0; i < n; i++) atomicAdd_system(p + 1, 1ULL); // system scope
}
int main() {
	char name[64]; snprintf(name, sizeof(name), "/gpuenv-atom-%d", (int)getpid());
	int fd = shm_open(name, O_CREAT | O_RDWR, 0600); ftruncate(fd, 1 << 20);
	void *p = mmap(nullptr, 1 << 20, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); shm_unlink(name);
	((volatile char *)p)[0] = 0;
	cudaSetDeviceFlags(cudaDeviceMapHost);
	printf("register: %s\n", cudaGetErrorString(cudaHostRegister(p, 1 << 20, cudaHostRegisterMapped)));
	unsigned long long *h = (unsigned long long *)p, *d = nullptr;
	cudaHostGetDevicePointer((void **)&d, p, 0);
	h[0] = h[1] = 0;
	add<<<4, 256>>>(d, 100);
	cudaError_t e = cudaDeviceSynchronize();
	printf("sync=%s device-scope atomicAdd=%llu system-scope atomicAdd=%llu (expect 102400 each)\n", cudaGetErrorString(e), h[0], h[1]);
	return 0;
}

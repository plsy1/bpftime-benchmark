// Does cudaHostGetDevicePointer work on an UNREGISTERED MAP_SHARED shm mapping?
// (bpf_attach_ctx_cuda.cpp:412 in a process that created the segment itself,
//  e.g. runtime unit test "CUDA watcher stops before shared-memory teardown")
#include <cuda_runtime.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
int main() {
	char name[64]; snprintf(name, sizeof(name), "/gpuenv-unreg-%d", (int)getpid());
	int fd = shm_open(name, O_CREAT | O_RDWR, 0600); ftruncate(fd, 1 << 20);
	void *p = mmap(nullptr, 1 << 20, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); shm_unlink(name);
	((volatile char *)p)[0] = 1;
	cudaSetDeviceFlags(cudaDeviceMapHost);
	void *d = nullptr;
	cudaError_t e = cudaHostGetDevicePointer(&d, p, 0);
	printf("cudaHostGetDevicePointer(unregistered shm): %d %s dev=%p\n", (int)e, cudaGetErrorString(e), d);
	return 0;
}

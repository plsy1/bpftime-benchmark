// misc.cu - small edge cases around host registration that bpftime's agent can hit
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <cuda_runtime.h>

#define CK(x)                                                                                                          \
	({                                                                                                             \
		cudaError_t e_ = (x);                                                                                  \
		printf("    %-72s -> %d %s\n", #x, (int)e_, cudaGetErrorString(e_));                                   \
		e_;                                                                                                    \
	})
__global__ void k_read(const unsigned *p, unsigned *out) { *out = p[0] + p[1024]; }

int main()
{
	printf("[1] cudaSetDeviceFlags(cudaDeviceMapHost) after the context exists (agent case)\n");
	CK(cudaFree(0));
	CK(cudaSetDeviceFlags(cudaDeviceMapHost));
	CK(cudaGetLastError());
	unsigned fl = 0;
	CK(cudaGetDeviceFlags(&fl));
	printf("    device flags now 0x%x (cudaDeviceMapHost=0x%x)\n", fl, cudaDeviceMapHost);

	std::string path = "/dev/shm/gpu-env-misc-" + std::to_string(getpid());
	int fd = open(path.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
	ftruncate(fd, 1 << 20);
	unsigned *p = (unsigned *)mmap(nullptr, 1 << 20, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	p[0] = 5;
	p[1024] = 7;

	printf("[2] register from a non-main thread, use from main thread\n");
	cudaError_t te = cudaErrorUnknown;
	std::thread th([&] { te = cudaHostRegister(p, 1 << 20, cudaHostRegisterMapped); });
	th.join();
	printf("    cudaHostRegister in std::thread -> %d %s\n", (int)te, cudaGetErrorString(te));
	void *d = nullptr;
	CK(cudaHostGetDevicePointer(&d, p, 0));
	unsigned *out;
	cudaMalloc(&out, 4);
	k_read<<<1, 1>>>((const unsigned *)d, out);
	unsigned v = 0;
	CK(cudaDeviceSynchronize());
	cudaMemcpy(&v, out, 4, cudaMemcpyDeviceToHost);
	printf("    kernel read p[0]+p[1024] = %u (expected 12)\n", v);

	printf("[3] register a sub-range of an already registered range, and the same range again\n");
	CK(cudaHostRegister((char *)p + 8192, 8192, cudaHostRegisterMapped));
	CK(cudaGetLastError());
	CK(cudaHostRegister(p, 1 << 20, cudaHostRegisterMapped));
	CK(cudaGetLastError());
	CK(cudaHostUnregister(p));

	printf("[4] PROT_READ MAP_SHARED mapping\n");
	unsigned *ro = (unsigned *)mmap(nullptr, 1 << 20, PROT_READ, MAP_SHARED, fd, 0);
	CK(cudaHostRegister(ro, 1 << 20, cudaHostRegisterMapped));
	CK(cudaGetLastError());
	if (CK(cudaHostRegister(ro, 1 << 20, cudaHostRegisterMapped | cudaHostRegisterReadOnly)) == cudaSuccess) {
		void *dr = nullptr;
		CK(cudaHostGetDevicePointer(&dr, ro, 0));
		k_read<<<1, 1>>>((const unsigned *)dr, out);
		CK(cudaDeviceSynchronize());
		cudaMemcpy(&v, out, 4, cudaMemcpyDeviceToHost);
		printf("    kernel read via read-only registration = %u (expected 12)\n", v);
		CK(cudaHostUnregister(ro));
	}
	CK(cudaGetLastError());
	munmap(ro, 1 << 20);
	munmap(p, 1 << 20);
	close(fd);
	unlink(path.c_str());
	return 0;
}

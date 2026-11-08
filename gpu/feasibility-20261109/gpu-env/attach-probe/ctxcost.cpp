// Cost, in any process an agent from a CUDA-attach build is loaded into, of
// bpftime_shm::register_cuda_host_memory (bpftime_shm_internal.cpp ~1395-1433):
// cudaSetDeviceFlags(MapHost) + prefault + cudaHostRegister(128 MiB segment).
#include <cuda_runtime.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
static long rss_kb()
{
	FILE *f = fopen("/proc/self/status", "r");
	char l[256];
	long v = -1;
	while (f && fgets(l, sizeof l, f))
		if (!strncmp(l, "VmRSS:", 6))
			sscanf(l + 6, "%ld", &v);
	if (f)
		fclose(f);
	return v;
}
int main()
{
	using clk = std::chrono::steady_clock;
	const size_t sz = 128u << 20;
	char name[64];
	snprintf(name, sizeof name, "/gpu-env-ctxcost-%d", getpid());
	int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
	ftruncate(fd, sz);
	posix_fallocate(fd, 0, sz);
	char *p = (char *)mmap(nullptr, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	shm_unlink(name);
	long r0 = rss_kb();
	auto t0 = clk::now();
	cudaError_t e1 = cudaSetDeviceFlags(cudaDeviceMapHost);
	volatile unsigned char s = 0;
	for (size_t i = 0; i < sz; i += 4096)
		s ^= p[i];
	auto t1 = clk::now();
	cudaError_t e2 = cudaHostRegister(p, sz, cudaHostRegisterMapped);
	auto t2 = clk::now();
	long r1 = rss_kb();
	printf("setDeviceFlags=%d prefault: %.1f ms | cudaHostRegister(128MiB)=%d: %.1f ms (includes context creation) | VmRSS %ld -> %ld MiB\n",
	       (int)e1, std::chrono::duration<double, std::milli>(t1 - t0).count(), (int)e2,
	       std::chrono::duration<double, std::milli>(t2 - t1).count(), r0 / 1024, r1 / 1024);
	cudaHostUnregister(p);
	return 0;
}

// kmap.cu - kernel BPF array map (BPF_F_MMAPABLE) mmapped into user space, then registered with
// cuMemHostRegister(PORTABLE|DEVICEMAP) like array_map_kernel_gpu.cpp:288-302 does. Needs CAP_BPF
// (unprivileged_bpf_disabled=1 on this board), so run it as: timeout 60 sudo ./kmap
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <linux/bpf.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cuda.h>
#include <cuda_runtime.h>

static long sys_bpf(int cmd, union bpf_attr *a) { return syscall(__NR_bpf, cmd, a, sizeof(*a)); }
__global__ void k_fill(unsigned long long *v, int n)
{
	int i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i < n)
		v[i] = 0x1000ull + i;
}
int main()
{
	const int N = 1024;
	union bpf_attr a;
	memset(&a, 0, sizeof a);
	a.map_type = BPF_MAP_TYPE_ARRAY;
	a.key_size = 4;
	a.value_size = 8;
	a.max_entries = N;
	a.map_flags = BPF_F_MMAPABLE;
	int fd = (int)sys_bpf(BPF_MAP_CREATE, &a);
	if (fd < 0) {
		printf("BPF_MAP_CREATE failed: %s (need CAP_BPF; unprivileged_bpf_disabled)\n", strerror(errno));
		return 1;
	}
	size_t sz = ((size_t)8 * N + 4095) & ~(size_t)4095;
	void *p = mmap(nullptr, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) {
		printf("mmap map fd failed: %s\n", strerror(errno));
		return 1;
	}
	printf("map fd=%d mmapped %zu bytes at %p\n", fd, sz, p);
	cudaFree(0);
	const char *es = "?";
	CUresult r = cuMemHostRegister(p, sz, CU_MEMHOSTREGISTER_PORTABLE | CU_MEMHOSTREGISTER_DEVICEMAP);
	cuGetErrorString(r, &es);
	printf("cuMemHostRegister(PORTABLE|DEVICEMAP) -> %d %s\n", (int)r, es);
	if (r != CUDA_SUCCESS) {
		cudaError_t e = cudaHostRegister(p, sz, cudaHostRegisterMapped);
		printf("cudaHostRegister(Mapped) -> %d %s\n", (int)e, cudaGetErrorString(e));
		return 1;
	}
	CUdeviceptr d = 0;
	r = cuMemHostGetDevicePointer(&d, p, 0);
	cuGetErrorString(r, &es);
	printf("cuMemHostGetDevicePointer -> %d %s dev=0x%llx\n", (int)r, es, (unsigned long long)d);
	k_fill<<<(N + 255) / 256, 256>>>((unsigned long long *)d, N);
	cudaError_t se = cudaDeviceSynchronize();
	printf("kernel fill: %s; CPU via mmap: v[0]=0x%llx v[%d]=0x%llx\n", cudaGetErrorString(se),
	       ((unsigned long long *)p)[0], N - 1, ((unsigned long long *)p)[N - 1]);
	int bad = 0;
	for (unsigned key = 0; key < (unsigned)N; key++) {
		unsigned long long val = 0;
		memset(&a, 0, sizeof a);
		a.map_fd = fd;
		a.key = (unsigned long long)(uintptr_t)&key;
		a.value = (unsigned long long)(uintptr_t)&val;
		if (sys_bpf(BPF_MAP_LOOKUP_ELEM, &a) || val != 0x1000ull + key)
			bad++;
	}
	printf("BPF_MAP_LOOKUP_ELEM (kernel side view) mismatches=%d of %d\n", bad, N);
	r = cuMemHostUnregister(p);
	cuGetErrorString(r, &es);
	printf("cuMemHostUnregister -> %d %s\n", (int)r, es);
	munmap(p, sz);
	close(fd);
	return 0;
}

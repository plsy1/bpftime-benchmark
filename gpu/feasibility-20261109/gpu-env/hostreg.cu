// hostreg.cu - cudaHostRegister / mapped-memory probes shaped like bpftime's GPU usage.
// Modes:
//   malloc                      : register heap (aligned/unaligned), stack int, static int
//   shm <MiB> <flags> [nopf]    : /dev/shm MAP_SHARED + register; flags = mapped|default|portmap|drv
//   hs <mem> <variant> <xproc>  : GPU<->CPU flag handshake while the kernel runs
//                                 mem = shm|hostalloc|malloc, variant = bpf|vol|none, xproc = 0|1
//   atom                        : GPU atomics (and concurrent CPU atomics) on registered /dev/shm memory
//   dual                        : two processes each register their own mapping of one /dev/shm file
//   dual-b <path>               : (internal) second process for "dual"
//   hostptr                     : kernel dereferences the HOST pointer of registered memory (poisons ctx)
#include <atomic>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <libgen.h>
#include <spawn.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <cuda.h>
#include <cuda_runtime.h>

extern char **environ;
static char g_dir[PATH_MAX];

static double now_ms()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}
#define CK(x)                                                                                                          \
	({                                                                                                             \
		cudaError_t e_ = (x);                                                                                  \
		printf("    %-70s -> %d %s\n", #x, (int)e_, cudaGetErrorString(e_));                                   \
		e_;                                                                                                    \
	})
#define DK(x)                                                                                                          \
	({                                                                                                             \
		CUresult r_ = (x);                                                                                     \
		const char *s_ = "?";                                                                                  \
		cuGetErrorString(r_, &s_);                                                                             \
		printf("    %-70s -> %d %s\n", #x, (int)r_, s_);                                                       \
		r_;                                                                                                    \
	})

static int run(std::vector<std::string> args)
{
	std::vector<char *> av;
	for (auto &s : args)
		av.push_back((char *)s.c_str());
	av.push_back(nullptr);
	pid_t pid;
	int rc = posix_spawnp(&pid, av[0], nullptr, nullptr, av.data(), environ);
	if (rc) {
		printf("    posix_spawnp failed: %s\n", strerror(rc));
		return -1;
	}
	int st = 0;
	waitpid(pid, &st, 0);
	fflush(stdout);
	return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
}
static pid_t spawn_bg(std::vector<std::string> args)
{
	std::vector<char *> av;
	for (auto &s : args)
		av.push_back((char *)s.c_str());
	av.push_back(nullptr);
	pid_t pid;
	if (posix_spawnp(&pid, av[0], nullptr, nullptr, av.data(), environ))
		return -1;
	return pid;
}
static std::string tool() { return std::string(g_dir) + "/shmtool"; }

__global__ void k_touch(unsigned *p, size_t stride, size_t n, unsigned add)
{
	for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < n; i += (size_t)gridDim.x * blockDim.x)
		p[i * stride] += add;
}
__global__ void k_check(const unsigned *p, size_t stride, size_t n, unsigned base, unsigned long long *bad)
{
	for (size_t i = blockIdx.x * blockDim.x + threadIdx.x; i < n; i += (size_t)gridDim.x * blockDim.x)
		if (p[i * stride] != base + (unsigned)i)
			atomicAdd(bad, 1ull);
}

static size_t cpu_check(const unsigned *p, size_t stride, size_t n, unsigned add)
{
	size_t bad = 0;
	for (size_t i = 0; i < n; i++)
		if (p[i * stride] != (unsigned)(i + add))
			bad++;
	return bad;
}

// ---------------------------------------------------------------- malloc
static void reg_and_use(const char *tag, void *hp, size_t bytes, unsigned flags, bool drv)
{
	printf("[%s] host=%p bytes=%zu page_aligned=%d flags=0x%x %s\n", tag, hp, bytes, ((uintptr_t)hp & 4095) == 0,
	       flags, drv ? "(driver API cuMemHostRegister)" : "");
	unsigned *h = (unsigned *)hp;
	size_t n = bytes / sizeof(unsigned);
	for (size_t i = 0; i < n; i++)
		h[i] = (unsigned)i;
	void *d = nullptr;
	if (drv) {
		if (DK(cuMemHostRegister(hp, bytes, flags)) != CUDA_SUCCESS)
			return;
		CUdeviceptr dp = 0;
		if (DK(cuMemHostGetDevicePointer(&dp, hp, 0)) != CUDA_SUCCESS) {
			cuMemHostUnregister(hp);
			return;
		}
		d = (void *)dp;
	} else {
		if (CK(cudaHostRegister(hp, bytes, flags)) != cudaSuccess) {
			cudaGetLastError();
			return;
		}
		if (CK(cudaHostGetDevicePointer(&d, hp, 0)) != cudaSuccess) {
			cudaGetLastError();
			cudaHostUnregister(hp);
			return;
		}
	}
	printf("    device ptr=%p  (equal to host ptr: %s)\n", d, d == hp ? "yes" : "no");
	unsigned int f = 0;
	if (!drv && cudaHostGetFlags(&f, hp) == cudaSuccess)
		printf("    cudaHostGetFlags=0x%x\n", f);
	k_touch<<<1, 64>>>((unsigned *)d, 1, n, 0x100);
	cudaError_t le = cudaGetLastError();
	cudaError_t se = cudaDeviceSynchronize();
	printf("    kernel via device ptr: launch=%s sync=%s\n", cudaGetErrorString(le), cudaGetErrorString(se));
	printf("    CPU sees GPU writes: mismatches=%zu of %zu (h[0]=0x%x h[n-1]=0x%x)\n", cpu_check(h, 1, n, 0x100), n,
	       h[0], h[n - 1]);
	if (drv)
		DK(cuMemHostUnregister(hp));
	else
		CK(cudaHostUnregister(hp));
}

static int g_static_int = 7;
static int mode_malloc()
{
	CK(cudaSetDeviceFlags(cudaDeviceMapHost));
	CK(cudaFree(0));
	void *a = nullptr;
	posix_memalign(&a, 4096, 65536);
	reg_and_use("A posix_memalign 64KiB, cudaHostRegisterMapped", a, 65536, cudaHostRegisterMapped, false);
	reg_and_use("B posix_memalign 64KiB, cudaHostRegisterDefault", a, 65536, cudaHostRegisterDefault, false);
	reg_and_use("C posix_memalign 64KiB, Portable|Mapped", a, 65536, cudaHostRegisterPortable | cudaHostRegisterMapped,
		    false);
	reg_and_use("D posix_memalign 64KiB, cuMemHostRegister(PORTABLE|DEVICEMAP)", a, 65536,
		    CU_MEMHOSTREGISTER_PORTABLE | CU_MEMHOSTREGISTER_DEVICEMAP, true);
	free(a);
	void *m = malloc(40);
	reg_and_use("E malloc(40) unaligned, Mapped", m, 40, cudaHostRegisterMapped, false);
	free(m);
	void *big = malloc(1 << 20); // glibc: mmap-backed chunk, ptr = page+16
	reg_and_use("F malloc(1MiB) (mmap chunk, unaligned), Mapped", big, 1 << 20, cudaHostRegisterMapped, false);
	free(big);
	int buf = 11223344; // like default_trampoline.cu:582-584
	reg_and_use("G stack int (4 bytes), Mapped", &buf, sizeof(buf), cudaHostRegisterMapped, false);
	reg_and_use("H static .data int (4 bytes), Mapped", &g_static_int, sizeof(int), cudaHostRegisterMapped, false);
	cudaError_t e = cudaGetLastError();
	printf("final cudaGetLastError=%s\n", cudaGetErrorString(e));
	return 0;
}

// ---------------------------------------------------------------- shm
struct Shm {
	std::string path;
	unsigned *p = nullptr;
	size_t size = 0;
	int fd = -1;
};
static bool shm_create(Shm &s, size_t size, const char *tag)
{
	s.path = std::string("/dev/shm/gpu-env-") + tag + "-" + std::to_string(getpid());
	s.fd = open(s.path.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
	if (s.fd < 0) {
		perror("open shm");
		return false;
	}
	if (ftruncate(s.fd, size)) {
		perror("ftruncate");
		return false;
	}
	s.p = (unsigned *)mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, s.fd, 0);
	if (s.p == MAP_FAILED) {
		perror("mmap");
		return false;
	}
	s.size = size;
	printf("shm %s size=%zu mapped MAP_SHARED at %p\n", s.path.c_str(), size, (void *)s.p);
	return true;
}
static void shm_destroy(Shm &s)
{
	if (s.p && s.p != MAP_FAILED)
		munmap(s.p, s.size);
	if (s.fd >= 0)
		close(s.fd);
	unlink(s.path.c_str());
}

static int mode_shm(size_t mib, const char *flags, bool prefault)
{
	CK(cudaSetDeviceFlags(cudaDeviceMapHost)); // as bpftime_shm_internal.cpp:1405
	CK(cudaFree(0));
	Shm s;
	if (!shm_create(s, mib << 20, "shm"))
		return 1;
	const size_t stride = 1024, n = s.size / 4096; // one u32 per 4 KiB page
	if (prefault) {
		volatile unsigned char sink = 0;
		for (size_t i = 0; i < s.size; i += 4096)
			sink ^= ((volatile unsigned char *)s.p)[i];
	}
	for (size_t i = 0; i < n; i++)
		s.p[i * stride] = (unsigned)i;
	bool drv = !strcmp(flags, "drv");
	unsigned fl = !strcmp(flags, "mapped")	  ? cudaHostRegisterMapped
		      : !strcmp(flags, "default") ? cudaHostRegisterDefault
		      : !strcmp(flags, "portmap") ? (cudaHostRegisterPortable | cudaHostRegisterMapped)
						  : (CU_MEMHOSTREGISTER_PORTABLE | CU_MEMHOSTREGISTER_DEVICEMAP);
	printf("register %zu MiB flags=%s(0x%x) prefault=%d\n", mib, flags, fl, prefault);
	double t0 = now_ms();
	bool ok;
	void *d = nullptr, *di = nullptr, *dl = nullptr;
	if (drv) {
		ok = DK(cuMemHostRegister(s.p, s.size, fl)) == CUDA_SUCCESS;
	} else {
		ok = CK(cudaHostRegister(s.p, s.size, fl)) == cudaSuccess;
	}
	double t1 = now_ms();
	printf("    register took %.3f ms\n", t1 - t0);
	if (!ok) {
		shm_destroy(s);
		return 1;
	}
	char *hi = (char *)s.p + 4096 + 16, *hl = (char *)s.p + s.size - 4;
	if (drv) {
		CUdeviceptr a = 0, b = 0, c = 0;
		DK(cuMemHostGetDevicePointer(&a, s.p, 0));
		DK(cuMemHostGetDevicePointer(&b, hi, 0));
		DK(cuMemHostGetDevicePointer(&c, hl, 0));
		d = (void *)a, di = (void *)b, dl = (void *)c;
	} else {
		CK(cudaHostGetDevicePointer(&d, s.p, 0));
		CK(cudaHostGetDevicePointer(&di, hi, 0));
		CK(cudaHostGetDevicePointer(&dl, hl, 0));
	}
	printf("    host base=%p dev base=%p equal=%s\n", (void *)s.p, d, d == (void *)s.p ? "yes" : "no");
	printf("    interior +4112: dev=%p expected=%p %s\n", di, (char *)d + 4112, di == (char *)d + 4112 ? "OK" : "MISMATCH");
	printf("    last u32: dev=%p expected=%p %s\n", dl, (char *)d + s.size - 4,
	       dl == (char *)d + s.size - 4 ? "OK" : "MISMATCH");
	if (!drv) {
		unsigned int f = 0;
		cudaError_t ge = cudaHostGetFlags(&f, s.p);
		printf("    cudaHostGetFlags=0x%x (%s)\n", f, cudaGetErrorString(ge));
	}
	// GPU writes every page; CPU (same process) and a second process check
	double k0 = now_ms();
	k_touch<<<64, 256>>>((unsigned *)d, stride, n, 0x1000);
	cudaError_t le = cudaGetLastError(), se = cudaDeviceSynchronize();
	printf("    kernel k_touch over %zu pages: launch=%s sync=%s (%.3f ms)\n", n, cudaGetErrorString(le),
	       cudaGetErrorString(se), now_ms() - k0);
	printf("    same-process CPU check: mismatches=%zu of %zu\n", cpu_check(s.p, stride, n, 0x1000), n);
	int rc = run({"timeout", "10", tool(), "check", s.path, std::to_string(stride), std::to_string(n), "0x1000"});
	printf("    second-process check exit=%d\n", rc);
	// reverse: second process writes, GPU reads
	rc = run({"timeout", "10", tool(), "fill", s.path, std::to_string(stride), std::to_string(n), "0xA0000000"});
	printf("    second-process fill exit=%d\n", rc);
	unsigned long long *bad = nullptr, hbad = ~0ull;
	cudaMalloc(&bad, 8);
	cudaMemset(bad, 0, 8);
	k_check<<<64, 256>>>((const unsigned *)d, stride, n, 0xA0000000u, bad);
	le = cudaGetLastError(), se = cudaDeviceSynchronize();
	cudaMemcpy(&hbad, bad, 8, cudaMemcpyDeviceToHost);
	printf("    GPU reads second-process writes: launch=%s sync=%s mismatches=%llu of %zu\n", cudaGetErrorString(le),
	       cudaGetErrorString(se), hbad, n);
	cudaFree(bad);
	double u0 = now_ms();
	if (drv)
		DK(cuMemHostUnregister(s.p));
	else
		CK(cudaHostUnregister(s.p));
	printf("    unregister took %.3f ms\n", now_ms() - u0);
	shm_destroy(s);
	return 0;
}

// ---------------------------------------------------------------- handshake
__device__ __forceinline__ unsigned long long gtimer()
{
	unsigned long long t;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	return t;
}
// variant 0 = bpftime-exact (default_trampoline.cu:149-162): membar.sys; st.global; loop{membar.sys; ld.global}
// variant 1 = membar.sys; st.volatile.global; loop{ld.volatile.global}
// variant 2 = st.volatile.global; loop{ld.volatile.global}   (no fences at all)
__global__ void k_hs(unsigned *f1, unsigned *f2, int variant, unsigned long long to_ns, unsigned long long *out)
{
	unsigned long long t0 = gtimer(), spins = 0, seen = 0;
	if (variant == 0)
		asm volatile("membar.sys;\n\tst.global.u32 [%0], 1;" ::"l"(f1) : "memory");
	else if (variant == 1)
		asm volatile("membar.sys;\n\tst.volatile.global.u32 [%0], 1;" ::"l"(f1) : "memory");
	else
		asm volatile("st.volatile.global.u32 [%0], 1;" ::"l"(f1) : "memory");
	for (;;) {
		unsigned v;
		if (variant == 0)
			asm volatile("membar.sys;\n\tld.global.u32 %0, [%1];" : "=r"(v) : "l"(f2) : "memory");
		else
			asm volatile("ld.volatile.global.u32 %0, [%1];" : "=r"(v) : "l"(f2) : "memory");
		spins++;
		if (v != 0) {
			seen = 1;
			break;
		}
		if (gtimer() - t0 > to_ns)
			break;
	}
	unsigned long long t1 = gtimer();
	if (seen)
		asm volatile("st.global.u32 [%0], 0;\n\tmembar.sys;" ::"l"(f2) : "memory");
	out[0] = spins;
	out[1] = t1 - t0;
	out[2] = seen;
}

static int mode_hs(const char *mem, const char *var, int xproc)
{
	int variant = !strcmp(var, "bpf") ? 0 : !strcmp(var, "vol") ? 1 : 2;
	CK(cudaSetDeviceFlags(cudaDeviceMapHost));
	CK(cudaFree(0));
	Shm s;
	unsigned *h = nullptr;
	void *d = nullptr;
	const size_t off = 65536; // flags live inside the segment like CommSharedMem does
	if (!strcmp(mem, "shm")) {
		if (!shm_create(s, 1 << 20, "hs"))
			return 1;
		if (CK(cudaHostRegister(s.p, s.size, cudaHostRegisterMapped)) != cudaSuccess)
			return 1;
		void *db;
		CK(cudaHostGetDevicePointer(&db, s.p, 0));
		h = (unsigned *)((char *)s.p + off);
		d = (char *)db + off;
	} else if (!strcmp(mem, "hostalloc")) {
		void *hp;
		CK(cudaHostAlloc(&hp, 1 << 20, cudaHostAllocMapped));
		CK(cudaHostGetDevicePointer(&d, hp, 0));
		h = (unsigned *)((char *)hp + off);
		d = (char *)d + off;
	} else {
		void *hp = nullptr;
		posix_memalign(&hp, 4096, 1 << 20);
		CK(cudaHostRegister(hp, 1 << 20, cudaHostRegisterMapped));
		CK(cudaHostGetDevicePointer(&d, hp, 0));
		h = (unsigned *)((char *)hp + off);
		d = (char *)d + off;
	}
	h[0] = h[1] = 0; // flag1 at +0, flag2 at +4 (adjacent like CommSharedMem)
	__atomic_thread_fence(__ATOMIC_SEQ_CST);
	unsigned long long *out;
	cudaMalloc(&out, 3 * 8);
	cudaMemset(out, 0, 24);
	cudaDeviceSynchronize();
	printf("handshake mem=%s variant=%s(%d) xproc=%d host=%p dev=%p\n", mem, var, variant, xproc, (void *)h, d);
	std::atomic<int> host_seen{0};
	double host_ms = -1;
	unsigned long polls = 0;
	std::thread th;
	pid_t child = -1;
	if (xproc) {
		child = spawn_bg({"timeout", "10", tool(), "hs", s.path, std::to_string(off), std::to_string(off + 4), "5000"});
		usleep(200000); // let the child map the file first
	} else {
		th = std::thread([&] {
			double t0 = now_ms();
			while (__atomic_load_n(&h[0], __ATOMIC_ACQUIRE) != 1) {
				polls++;
				if (now_ms() - t0 > 5000)
					return;
			}
			host_ms = now_ms() - t0;
			__atomic_store_n(&h[0], 0, __ATOMIC_RELEASE);
			__atomic_store_n(&h[1], 1, __ATOMIC_RELEASE);
			host_seen = 1;
		});
	}
	double k0 = now_ms();
	k_hs<<<1, 1>>>((unsigned *)d, (unsigned *)d + 1, variant, 3000000000ull, out);
	cudaError_t le = cudaGetLastError(), se = cudaDeviceSynchronize();
	double k1 = now_ms();
	unsigned long long o[3] = {0, 0, 0};
	cudaMemcpy(o, out, 24, cudaMemcpyDeviceToHost);
	if (xproc) {
		int st = 0;
		waitpid(child, &st, 0);
		printf("    child exit=%d\n", WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st));
	} else {
		th.join();
		printf("    host thread: saw flag1=%s after %.3f ms (%lu polls)\n", host_seen ? "yes" : "NO", host_ms, polls);
	}
	printf("    kernel: launch=%s sync=%s saw flag2=%s spins=%llu elapsed_in_kernel=%.3f us; wall %.3f ms\n",
	       cudaGetErrorString(le), cudaGetErrorString(se), o[2] ? "yes" : "NO", o[0], o[1] / 1000.0, k1 - k0);
	printf("    final flags: flag1=%u flag2=%u\n", h[0], h[1]);
	if (!strcmp(mem, "shm")) {
		cudaHostUnregister(s.p);
		shm_destroy(s);
	}
	return 0;
}

// ---------------------------------------------------------------- atomics
__global__ void k_atom(unsigned *c32, unsigned long long *c64, int iters)
{
	for (int i = 0; i < iters; i++) {
		atomicAdd(c32, 1u);
		atomicAdd(c64, 1ull);
	}
}
__global__ void k_atom_sys(unsigned *c32, int iters)
{
	for (int i = 0; i < iters; i++)
		atomicAdd_system(c32, 1u);
}
__global__ void k_cas_lock(int *lock, unsigned *ctr, int iters)
{ // per-thread CAS spinlock on host memory (one lane per warp to avoid intra-warp deadlock)
	if ((threadIdx.x & 31) != 0)
		return;
	for (int i = 0; i < iters; i++) {
		while (atomicCAS(lock, 0, 1) != 0) {
		}
		__threadfence_system();
		unsigned v = *(volatile unsigned *)ctr;
		*(volatile unsigned *)ctr = v + 1;
		__threadfence_system();
		atomicExch(lock, 0);
	}
}
static int mode_atom()
{
	CK(cudaSetDeviceFlags(cudaDeviceMapHost));
	CK(cudaFree(0));
	Shm s;
	if (!shm_create(s, 1 << 20, "atom"))
		return 1;
	memset(s.p, 0, s.size);
	if (CK(cudaHostRegister(s.p, s.size, cudaHostRegisterMapped)) != cudaSuccess)
		return 1;
	char *db;
	CK(cudaHostGetDevicePointer((void **)&db, s.p, 0));
	unsigned *h32 = (unsigned *)s.p;
	unsigned long long *h64 = (unsigned long long *)((char *)s.p + 64);
	const int B = 8, T = 256, IT = 100;
	double t0 = now_ms();
	k_atom<<<B, T>>>((unsigned *)db, (unsigned long long *)(db + 64), IT);
	cudaError_t se = cudaDeviceSynchronize();
	printf("[atom GPU-only] atomicAdd u32/u64 on registered shm: sync=%s u32=%u u64=%llu expected=%d (%.3f ms)\n",
	       cudaGetErrorString(se), *h32, *h64, B * T * IT, now_ms() - t0);
	// concurrent CPU + GPU system-scope atomics on the same word
	unsigned *hc = (unsigned *)((char *)s.p + 128);
	*hc = 0;
	const int GIT = 1000, CPU_N = 2000000;
	std::atomic<int> go{0};
	double cpu_t0 = 0, cpu_t1 = 0;
	std::thread th([&] {
		while (!go.load()) {
		}
		cpu_t0 = now_ms();
		for (int i = 0; i < CPU_N; i++)
			__atomic_fetch_add(hc, 1u, __ATOMIC_RELAXED);
		cpu_t1 = now_ms();
	});
	double g0 = now_ms();
	k_atom_sys<<<B, T>>>((unsigned *)(db + 128), GIT);
	go = 1;
	se = cudaDeviceSynchronize();
	double g1 = now_ms();
	th.join();
	unsigned long long expect = (unsigned long long)B * T * GIT + CPU_N;
	printf("[atom CPU+GPU] atomicAdd_system vs __atomic_fetch_add: sync=%s total=%u expected=%llu lost=%lld; gpu "
	       "window %.1f-%.1f ms, cpu window %.1f-%.1f ms (relative)\n",
	       cudaGetErrorString(se), *hc, expect, (long long)expect - (long long)*hc, 0.0, g1 - g0, cpu_t0 - g0,
	       cpu_t1 - g0);
	// CAS spinlock living in host memory (like a map spinlock would)
	int *hl = (int *)((char *)s.p + 256);
	unsigned *hctr = (unsigned *)((char *)s.p + 320);
	*hl = 0;
	*hctr = 0;
	k_cas_lock<<<8, 128>>>((int *)(db + 256), (unsigned *)(db + 320), 50);
	se = cudaDeviceSynchronize();
	printf("[atom CAS lock in host mem] sync=%s counter=%u expected=%d lock=%d\n", cudaGetErrorString(se), *hctr,
	       8 * 4 * 50, *hl);
	cudaHostUnregister(s.p);
	shm_destroy(s);
	return 0;
}

// ---------------------------------------------------------------- multi-round handshake with payload
// Layout mirrors CommSharedMem: flag1, flag2, occupy, request_id, ..., resp (written by host before flag2).
struct HsMem {
	unsigned flag1, flag2, occupy, request_id;
	unsigned long long resp[4];
};
__global__ void k_hsloop(HsMem *g, int variant, int rounds, unsigned long long to_ns, unsigned long long *out)
{
	unsigned long long t0 = gtimer(), stale = 0, done = 0, maxrt = 0;
	for (int r = 1; r <= rounds; r++) {
		unsigned long long rt0 = gtimer();
		g->request_id = r; // plain store, like make_helper_call
		unsigned v = 0;
		if (variant == 0) {
			// exact bpftime sequence (default_trampoline.cu:149-162) but with a time bound
			asm volatile("membar.sys;\n\tst.global.u32 [%0], 1;" ::"l"(&g->flag1) : "memory");
			for (;;) {
				asm volatile("membar.sys;\n\tld.global.u32 %0, [%1];" : "=r"(v) : "l"(&g->flag2) : "memory");
				if (v || gtimer() - t0 > to_ns)
					break;
			}
			if (!v)
				break;
			asm volatile("st.global.u32 [%0], 0;\n\tmembar.sys;" ::"l"(&g->flag2) : "memory");
		} else {
			__threadfence_system();
			*(volatile unsigned *)&g->flag1 = 1;
			for (;;) {
				v = *(volatile unsigned *)&g->flag2;
				if (v || gtimer() - t0 > to_ns)
					break;
			}
			if (!v)
				break;
			*(volatile unsigned *)&g->flag2 = 0;
			__threadfence_system();
		}
		unsigned long long resp = g->resp[0]; // plain (weak) load, like `my_resp = g_data->resp;`
		if (resp != (unsigned long long)r * 3 + 1)
			stale++;
		done++;
		unsigned long long rt = gtimer() - rt0;
		if (rt > maxrt)
			maxrt = rt;
	}
	out[0] = done;
	out[1] = stale;
	out[2] = gtimer() - t0;
	out[3] = maxrt;
}
static int mode_hsloop(const char *var, int rounds, int xmem)
{
	int variant = !strcmp(var, "bpf") ? 0 : 1;
	CK(cudaSetDeviceFlags(cudaDeviceMapHost));
	CK(cudaFree(0));
	Shm s;
	if (!shm_create(s, 1 << 20, "hsloop"))
		return 1;
	memset(s.p, 0, s.size);
	if (CK(cudaHostRegister(s.p, s.size, cudaHostRegisterMapped)) != cudaSuccess)
		return 1;
	char *db;
	CK(cudaHostGetDevicePointer((void **)&db, s.p, 0));
	HsMem *h = (HsMem *)((char *)s.p + 65536);
	HsMem *d = (HsMem *)(db + 65536);
	unsigned long long *out;
	cudaMalloc(&out, 32);
	cudaMemset(out, 0, 32);
	cudaDeviceSynchronize();
	std::atomic<int> stop{0};
	unsigned long served = 0;
	std::thread th([&] {
		double t0 = now_ms();
		while (!stop.load(std::memory_order_relaxed) && now_ms() - t0 < 10000) {
			if (__atomic_load_n(&h->flag1, __ATOMIC_ACQUIRE) == 1) {
				__atomic_store_n(&h->flag1, 0, __ATOMIC_RELAXED);
				unsigned id = __atomic_load_n(&h->request_id, __ATOMIC_RELAXED);
				h->resp[0] = (unsigned long long)id * 3 + 1;
				__atomic_store_n(&h->flag2, 1, __ATOMIC_RELEASE);
				served++;
			}
		}
	});
	double k0 = now_ms();
	k_hsloop<<<1, 1>>>(d, variant, rounds, 8000000000ull, out);
	cudaError_t le = cudaGetLastError(), se = cudaDeviceSynchronize();
	double k1 = now_ms();
	stop = 1;
	th.join();
	unsigned long long o[4];
	cudaMemcpy(o, out, 32, cudaMemcpyDeviceToHost);
	printf("[hsloop variant=%s rounds=%d] launch=%s sync=%s done=%llu stale_resp=%llu host_served=%lu "
	       "kernel_total=%.3f ms avg_roundtrip=%.2f us max_roundtrip=%.2f us wall=%.3f ms\n",
	       var, rounds, cudaGetErrorString(le), cudaGetErrorString(se), o[0], o[1], served, o[2] / 1e6,
	       o[0] ? o[2] / 1e3 / o[0] : 0.0, o[3] / 1e3, k1 - k0);
	cudaHostUnregister(s.p);
	shm_destroy(s);
	(void)xmem;
	return 0;
}

// many threads, serialized like make_helper_call (default_trampoline.cu:130-172): per-lane loop + device-memory
// spinlock + bpftime-exact flag asm; each thread checks the response written by the host for its request id.
__device__ int g_comm_lock = 0;
__global__ void k_hsmulti(HsMem *g, unsigned long long to_ns, unsigned long long *out)
{
	unsigned long long t0 = gtimer();
	unsigned id = blockIdx.x * blockDim.x + threadIdx.x + 1;
	int lane = threadIdx.x & 31;
	bool ok = false, timed_out = false;
	for (int active_lane = 0; active_lane < 32; active_lane++) {
		unsigned mask = __activemask();
		if (((mask >> active_lane) & 1) && lane == active_lane) {
			while (atomicCAS(&g_comm_lock, 0, 1) == 1) {
				if (gtimer() - t0 > to_ns) {
					timed_out = true;
					break;
				}
			}
			if (!timed_out) {
				g->request_id = id;
				unsigned v = 0;
				asm volatile("membar.sys;\n\tst.global.u32 [%0], 1;" ::"l"(&g->flag1) : "memory");
				for (;;) {
					asm volatile("membar.sys;\n\tld.global.u32 %0, [%1];" : "=r"(v) : "l"(&g->flag2) : "memory");
					if (v || gtimer() - t0 > to_ns)
						break;
				}
				if (v) {
					asm volatile("st.global.u32 [%0], 0;\n\tmembar.sys;" ::"l"(&g->flag2) : "memory");
					ok = (g->resp[0] == (unsigned long long)id * 3 + 1);
				} else {
					timed_out = true;
				}
				atomicExch(&g_comm_lock, 0);
			}
		}
		__syncwarp(mask);
	}
	atomicAdd(&out[ok ? 0 : 1], 1ull);
	if (timed_out)
		atomicAdd(&out[2], 1ull);
}
static int mode_hsmulti(int blocks, int threads)
{
	CK(cudaSetDeviceFlags(cudaDeviceMapHost));
	CK(cudaFree(0));
	Shm s;
	if (!shm_create(s, 1 << 20, "hsmulti"))
		return 1;
	memset(s.p, 0, s.size);
	if (CK(cudaHostRegister(s.p, s.size, cudaHostRegisterMapped)) != cudaSuccess)
		return 1;
	char *db;
	CK(cudaHostGetDevicePointer((void **)&db, s.p, 0));
	HsMem *h = (HsMem *)((char *)s.p + 65536);
	HsMem *d = (HsMem *)(db + 65536);
	unsigned long long *out;
	cudaMalloc(&out, 24);
	cudaMemset(out, 0, 24);
	cudaDeviceSynchronize();
	std::atomic<int> stop{0};
	unsigned long served = 0;
	std::thread th([&] {
		double t0 = now_ms();
		while (!stop.load(std::memory_order_relaxed) && now_ms() - t0 < 20000) {
			if (__atomic_load_n(&h->flag1, __ATOMIC_ACQUIRE) == 1) {
				__atomic_store_n(&h->flag1, 0, __ATOMIC_RELAXED);
				unsigned id = __atomic_load_n(&h->request_id, __ATOMIC_RELAXED);
				h->resp[0] = (unsigned long long)id * 3 + 1;
				__atomic_store_n(&h->flag2, 1, __ATOMIC_RELEASE);
				served++;
			}
		}
	});
	double k0 = now_ms();
	k_hsmulti<<<blocks, threads>>>(d, 15000000000ull, out);
	cudaError_t le = cudaGetLastError(), se = cudaDeviceSynchronize();
	double k1 = now_ms();
	stop = 1;
	th.join();
	unsigned long long o[3];
	cudaMemcpy(o, out, 24, cudaMemcpyDeviceToHost);
	printf("[hsmulti %dx%d] launch=%s sync=%s ok=%llu bad=%llu timed_out=%llu host_served=%lu wall=%.3f ms "
	       "(%.2f us per call)\n",
	       blocks, threads, cudaGetErrorString(le), cudaGetErrorString(se), o[0], o[1], o[2], served, k1 - k0,
	       served ? (k1 - k0) * 1000.0 / served : 0.0);
	cudaHostUnregister(s.p);
	shm_destroy(s);
	return 0;
}

// device-scope (non-_system) atomics on host memory concurrently with CPU atomics: this is what
// LLVM NVPTX emits for eBPF atomic adds (atom.global.add without .sys)
__global__ void k_atom_devscope(unsigned *c32, unsigned long long *c64, int iters)
{
	for (int i = 0; i < iters; i++) {
		atomicAdd(c32, 1u);
		atomicAdd(c64, 1ull);
	}
}
static int mode_atomdev()
{
	CK(cudaSetDeviceFlags(cudaDeviceMapHost));
	CK(cudaFree(0));
	Shm s;
	if (!shm_create(s, 1 << 20, "atomdev"))
		return 1;
	memset(s.p, 0, s.size);
	if (CK(cudaHostRegister(s.p, s.size, cudaHostRegisterMapped)) != cudaSuccess)
		return 1;
	char *db;
	CK(cudaHostGetDevicePointer((void **)&db, s.p, 0));
	unsigned *h32 = (unsigned *)s.p;
	unsigned long long *h64 = (unsigned long long *)((char *)s.p + 64);
	const int B = 8, T = 256, GIT = 1000, CPU_N = 2000000;
	std::atomic<int> go{0};
	double c0 = 0, c1 = 0;
	std::thread th([&] {
		while (!go.load()) {
		}
		c0 = now_ms();
		for (int i = 0; i < CPU_N; i++) {
			__atomic_fetch_add(h32, 1u, __ATOMIC_RELAXED);
			__atomic_fetch_add(h64, 1ull, __ATOMIC_RELAXED);
		}
		c1 = now_ms();
	});
	double g0 = now_ms();
	k_atom_devscope<<<B, T>>>((unsigned *)db, (unsigned long long *)(db + 64), GIT);
	go = 1;
	cudaError_t se = cudaDeviceSynchronize();
	double g1 = now_ms();
	th.join();
	unsigned long long ex = (unsigned long long)B * T * GIT + CPU_N;
	printf("[atomdev CPU+GPU device-scope atomicAdd] sync=%s u32=%u u64=%llu expected=%llu lost32=%lld lost64=%lld; "
	       "gpu 0.0-%.1f ms, cpu %.1f-%.1f ms\n",
	       cudaGetErrorString(se), *h32, *h64, ex, (long long)ex - (long long)*h32, (long long)ex - (long long)*h64,
	       g1 - g0, c0 - g0, c1 - g0);
	cudaHostUnregister(s.p);
	shm_destroy(s);
	return 0;
}

// ---------------------------------------------------------------- dual registration
static int mode_dual_b(const char *path)
{
	printf("[B pid=%d] start\n", getpid());
	int fd = open(path, O_RDWR);
	struct stat st;
	fstat(fd, &st);
	unsigned *p = (unsigned *)mmap(nullptr, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	CK(cudaSetDeviceFlags(cudaDeviceMapHost));
	if (CK(cudaHostRegister(p, st.st_size, cudaHostRegisterMapped)) != cudaSuccess)
		return 1;
	void *d;
	CK(cudaHostGetDevicePointer(&d, p, 0));
	printf("[B] host=%p dev=%p\n", (void *)p, d);
	size_t n = st.st_size / 4096;
	k_touch<<<16, 256>>>((unsigned *)d, 1024, n, 0x20000);
	printf("[B] k_touch +0x20000: sync=%s; B-CPU mismatches=%zu\n", cudaGetErrorString(cudaDeviceSynchronize()),
	       cpu_check(p, 1024, n, 0x20000), n);
	CK(cudaHostUnregister(p));
	munmap(p, st.st_size);
	close(fd);
	return 0;
}
static int mode_dual()
{
	CK(cudaSetDeviceFlags(cudaDeviceMapHost));
	CK(cudaFree(0));
	Shm s;
	if (!shm_create(s, 4 << 20, "dual"))
		return 1;
	size_t n = s.size / 4096;
	for (size_t i = 0; i < n; i++)
		s.p[i * 1024] = (unsigned)i;
	if (CK(cudaHostRegister(s.p, s.size, cudaHostRegisterMapped)) != cudaSuccess)
		return 1;
	void *d;
	CK(cudaHostGetDevicePointer(&d, s.p, 0));
	printf("[A pid=%d] host=%p dev=%p registered; spawning B while A holds its registration\n", getpid(),
	       (void *)s.p, d);
	int rc = run({"timeout", "30", std::string(g_dir) + "/hostreg", "dual-b", s.path});
	printf("[A] B exit=%d\n", rc);
	printf("[A] A-CPU sees B-GPU writes: mismatches=%zu of %zu\n", cpu_check(s.p, 1024, n, 0x20000), n);
	unsigned long long *bad, hb = ~0ull;
	cudaMalloc(&bad, 8);
	cudaMemset(bad, 0, 8);
	k_check<<<16, 256>>>((const unsigned *)d, 1024, n, 0x20000, bad);
	cudaError_t se = cudaDeviceSynchronize();
	cudaMemcpy(&hb, bad, 8, cudaMemcpyDeviceToHost);
	printf("[A] A-GPU sees B-GPU writes: sync=%s mismatches=%llu of %zu\n", cudaGetErrorString(se), hb, n);
	CK(cudaHostUnregister(s.p));
	shm_destroy(s);
	return 0;
}

// ---------------------------------------------------------------- host pointer used directly on device
static int mode_hostptr()
{
	CK(cudaSetDeviceFlags(cudaDeviceMapHost));
	CK(cudaFree(0));
	Shm s;
	if (!shm_create(s, 1 << 20, "hostptr"))
		return 1;
	for (size_t i = 0; i < 256; i++)
		s.p[i * 1024] = (unsigned)i;
	CK(cudaHostRegister(s.p, s.size, cudaHostRegisterMapped));
	void *d;
	CK(cudaHostGetDevicePointer(&d, s.p, 0));
	printf("host=%p dev=%p; launching kernel on the HOST pointer\n", (void *)s.p, d);
	k_touch<<<1, 256>>>(s.p, 1024, 256, 0x5);
	cudaError_t le = cudaGetLastError(), se = cudaDeviceSynchronize();
	printf("    launch=%s sync=%s CPU mismatches(+5)=%zu\n", cudaGetErrorString(le), cudaGetErrorString(se),
	       cpu_check(s.p, 1024, 256, 0x5));
	cudaHostUnregister(s.p);
	shm_destroy(s);
	return 0;
}

int main(int argc, char **argv)
{
	char exe[PATH_MAX];
	ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
	exe[n > 0 ? n : 0] = 0;
	strcpy(g_dir, dirname(exe));
	setvbuf(stdout, nullptr, _IOLBF, 0);
	if (argc < 2) {
		fprintf(stderr, "usage: see header\n");
		return 2;
	}
	std::string m = argv[1];
	if (m == "malloc")
		return mode_malloc();
	if (m == "shm" && argc >= 4)
		return mode_shm(strtoull(argv[2], 0, 0), argv[3], !(argc >= 5 && !strcmp(argv[4], "nopf")));
	if (m == "hs" && argc >= 5)
		return mode_hs(argv[2], argv[3], atoi(argv[4]));
	if (m == "atom")
		return mode_atom();
	if (m == "dual")
		return mode_dual();
	if (m == "dual-b" && argc >= 3)
		return mode_dual_b(argv[2]);
	if (m == "hostptr")
		return mode_hostptr();
	if (m == "hsloop" && argc >= 4)
		return mode_hsloop(argv[2], atoi(argv[3]), 0);
	if (m == "atomdev")
		return mode_atomdev();
	if (m == "hsmulti" && argc >= 4)
		return mode_hsmulti(atoi(argv[2]), atoi(argv[3]));
	fprintf(stderr, "bad args\n");
	return 2;
}

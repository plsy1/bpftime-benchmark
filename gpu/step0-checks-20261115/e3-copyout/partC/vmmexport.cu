// vmmexport.cu: step 0, check 3, part C (standalone, no bpftime).
//
// Process 1 (exporter) allocates device memory with cuMemCreate
// (requestedHandleTypes = POSIX file descriptor), maps it, writes a pattern
// with a kernel, exports it with cuMemExportToShareableHandle and passes the
// fd to process 2 over a Unix socket (SCM_RIGHTS). Process 2 (importer)
// imports, reserves, maps, sets access, then
//   (i)   reads it with a kernel,
//   (ii)  copies it with cuMemcpyDtoH (pageable and registered host memory),
//   (iii) tries to make it CPU-accessible: mmap of the fd, cuMemSetAccess
//         with a host location, cuPointerGetAttribute(HOST_POINTER).
// Then process 1 keeps rewriting the buffer with an increasing generation
// while process 2 samples it (DtoH, kernel, CPU mapping if any): does
// process 2 see the updates? Every driver call's CUresult is reported.
//
// The two processes come from one fork() made before any CUDA call.
//   vmmexport [--bytes N] [--seconds S] [--update-us U]
//             [--fill plain|wt|sysfence] [--rdma 0|1] [--out FILE]
// --rdma 1 sets allocFlags.gpuDirectRDMACapable in process 1's allocation.
// --fill picks how process 1 writes: plain stores, st.global.wt stores, or
// plain stores followed by __threadfence_system() in every thread.
// Word i of the buffer holds (generation << 32) | i.
#include <cuda.h>
#include <cuda_runtime.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <unistd.h>

#include "../common/regmem.h"

// ------------------------------------------------------------ JSON record
struct Rec {
	std::string role;
	std::vector<std::string> kv; // "\"k\":v"
	void cu(const char *name, CUresult r)
	{
		char b[256];
		snprintf(b, sizeof(b), "\"%s\":{\"rc\":%d,\"name\":\"%s\"}", name,
			 (int)r, cu_name(r));
		kv.push_back(b);
	}
	void num(const char *name, double v)
	{
		char b[256];
		snprintf(b, sizeof(b), "\"%s\":%.6g", name, v);
		kv.push_back(b);
	}
	void u64(const char *name, unsigned long long v)
	{
		char b[256];
		snprintf(b, sizeof(b), "\"%s\":%llu", name, v);
		kv.push_back(b);
	}
	void str(const char *name, const char *v)
	{
		std::string s = "\"";
		s += name;
		s += "\":\"";
		for (const char *p = v; *p; ++p) {
			if (*p == '"' || *p == '\\')
				s += '\\';
			if ((unsigned char)*p >= 32)
				s += *p;
		}
		s += "\"";
		kv.push_back(s);
	}
	void write(const char *path)
	{
		FILE *f = fopen(path, "a");
		if (!f)
			return;
		fprintf(f, "{\"test\":\"vmmexport\",\"role\":\"%s\"", role.c_str());
		for (auto &s : kv)
			fprintf(f, ",%s", s.c_str());
		fprintf(f, "}\n");
		fclose(f);
	}
};

// ------------------------------------------------------------ kernels
// mode 0: plain stores; 1: st.global.wt (write-through hint);
// 2: plain stores, then __threadfence_system() (fence.sc.sys) per thread
__global__ void fill(unsigned long long *p, size_t n, unsigned long long gen,
		     int mode)
{
	for (size_t i = blockIdx.x * (size_t)blockDim.x + threadIdx.x; i < n;
	     i += (size_t)gridDim.x * blockDim.x) {
		const unsigned long long v =
			(gen << 32) | (unsigned long long)(i & 0xffffffffu);
		if (mode == 1)
			__stwt(p + i, v);
		else
			p[i] = v;
	}
	if (mode == 2)
		__threadfence_system();
}

// counts words that do not hold gen's pattern
__global__ void check(const unsigned long long *p, size_t n,
		      unsigned long long gen, unsigned long long *bad)
{
	unsigned long long b = 0;
	for (size_t i = blockIdx.x * (size_t)blockDim.x + threadIdx.x; i < n;
	     i += (size_t)gridDim.x * blockDim.x)
		b += p[i] != ((gen << 32) | (unsigned long long)(i & 0xffffffffu));
	if (b)
		atomicAdd(bad, b);
}

// samples the first and last word (volatile loads) into out[0..1]
__global__ void sample(const volatile unsigned long long *p, size_t n,
		       unsigned long long *out)
{
	out[0] = p[0];
	out[1] = p[n - 1];
}

// ------------------------------------------------------------ fd passing
static int send_fd(int sock, int fd, uint64_t size)
{
	struct msghdr msg;
	memset(&msg, 0, sizeof(msg));
	struct iovec iov = { &size, sizeof(size) };
	char cbuf[CMSG_SPACE(sizeof(int))];
	memset(cbuf, 0, sizeof(cbuf));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cbuf;
	msg.msg_controllen = sizeof(cbuf);
	struct cmsghdr *c = CMSG_FIRSTHDR(&msg);
	c->cmsg_level = SOL_SOCKET;
	c->cmsg_type = SCM_RIGHTS;
	c->cmsg_len = CMSG_LEN(sizeof(int));
	memcpy(CMSG_DATA(c), &fd, sizeof(int));
	return sendmsg(sock, &msg, 0) == (ssize_t)sizeof(size) ? 0 : -1;
}

static int recv_fd(int sock, uint64_t *size)
{
	struct msghdr msg;
	memset(&msg, 0, sizeof(msg));
	struct iovec iov = { size, sizeof(*size) };
	char cbuf[CMSG_SPACE(sizeof(int))];
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cbuf;
	msg.msg_controllen = sizeof(cbuf);
	if (recvmsg(sock, &msg, 0) != (ssize_t)sizeof(*size))
		return -1;
	struct cmsghdr *c = CMSG_FIRSTHDR(&msg);
	if (!c || c->cmsg_type != SCM_RIGHTS)
		return -1;
	int fd;
	memcpy(&fd, CMSG_DATA(c), sizeof(int));
	return fd;
}

// wait up to ms for one byte; returns the byte, 0 on timeout, -1 on EOF
static int recv_byte(int sock, int ms)
{
	struct pollfd p = { sock, POLLIN, 0 };
	int r = poll(&p, 1, ms);
	if (r <= 0)
		return 0;
	char b;
	ssize_t n = read(sock, &b, 1);
	if (n <= 0)
		return -1;
	return (unsigned char)b;
}

static void describe_fd(Rec &R, int fd, const char *prefix)
{
	char key[128], link[512], path[64];
	struct stat sb;
	snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
	ssize_t n = readlink(path, link, sizeof(link) - 1);
	link[n > 0 ? n : 0] = 0;
	snprintf(key, sizeof(key), "%s_fd_link", prefix);
	R.str(key, link);
	if (fstat(fd, &sb) == 0) {
		snprintf(key, sizeof(key), "%s_fd_mode", prefix);
		R.u64(key, (unsigned long long)sb.st_mode);
		snprintf(key, sizeof(key), "%s_fd_size", prefix);
		R.u64(key, (unsigned long long)sb.st_size);
		snprintf(key, sizeof(key), "%s_fd_rdev", prefix);
		R.u64(key, (unsigned long long)sb.st_rdev);
	}
}

// ------------------------------------------------------------ guarded CPU read
static sigjmp_buf g_jb;
static volatile sig_atomic_t g_sig;
static void on_fault(int s)
{
	g_sig = s;
	siglongjmp(g_jb, 1);
}

// returns 0 if both words were read, else the signal number
static int guarded_read2(const volatile unsigned long long *p, size_t n,
			 unsigned long long out[2])
{
	struct sigaction sa, o1, o2;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_fault;
	sigaction(SIGSEGV, &sa, &o1);
	sigaction(SIGBUS, &sa, &o2);
	g_sig = 0;
	if (sigsetjmp(g_jb, 1) == 0) {
		out[0] = p[0];
		out[1] = p[n - 1];
	}
	sigaction(SIGSEGV, &o1, NULL);
	sigaction(SIGBUS, &o2, NULL);
	return g_sig;
}

// Counts words of a CPU mapping that do not hold gen's pattern. With civac,
// each 64-byte line is cleaned and invalidated in the CPU caches first
// (DC CIVAC, allowed at EL0 on Linux), so a stale CPU cache line cannot
// explain a mismatch. Returns ~0 if the scan faulted.
static unsigned long long cpu_scan(const volatile unsigned long long *p,
				   size_t n, unsigned long long gen, int civac)
{
	struct sigaction sa, o1, o2;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_fault;
	sigaction(SIGSEGV, &sa, &o1);
	sigaction(SIGBUS, &sa, &o2);
	g_sig = 0;
	volatile unsigned long long bad = 0;
	if (sigsetjmp(g_jb, 1) == 0) {
		for (size_t i = 0; i < n; ++i) {
			if (civac && (i & 7) == 0)
				asm volatile("dc civac, %0" ::"r"(p + i) : "memory");
			if (civac && (i & 7) == 0)
				asm volatile("dsb sy" ::: "memory");
			bad = bad + (p[i] != ((gen << 32) |
					     (unsigned long long)(i & 0xffffffffu)));
		}
	} else {
		bad = ~0ull;
	}
	sigaction(SIGSEGV, &o1, NULL);
	sigaction(SIGBUS, &o2, NULL);
	return bad;
}

struct Opts {
	size_t bytes = 16u << 20;
	double seconds = 2.0;
	long update_us = 2000;
	int fill_mode = 0; // plain | wt | sysfence
	int rdma = 0; // allocFlags.gpuDirectRDMACapable
	const char *out = "/dev/stdout";
};
static const char *fill_names[] = { "plain", "wt", "sysfence" };

// ------------------------------------------------------------ process 1
static int exporter(int sock, const Opts &o)
{
	Rec R;
	R.role = "exporter";
	alarm((unsigned)(o.seconds + 60));
	CUresult r = cuInit(0);
	R.cu("cuInit", r);
	cudaFree(0); // primary context current on this thread
	CUdevice dev = 0;
	cuDeviceGet(&dev, 0);
	CUmemAllocationProp prop;
	memset(&prop, 0, sizeof(prop));
	prop.type = CU_MEM_ALLOCATION_TYPE_PINNED;
	prop.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
	prop.location.id = 0;
	prop.requestedHandleTypes = CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR;
	prop.allocFlags.gpuDirectRDMACapable = (unsigned char)o.rdma;
	R.u64("rdma_flag", o.rdma);
	size_t gran = 0;
	R.cu("cuMemGetAllocationGranularity",
	     cuMemGetAllocationGranularity(&gran, &prop,
					   CU_MEM_ALLOC_GRANULARITY_MINIMUM));
	R.u64("granularity", gran);
	if (!gran)
		gran = 2u << 20;
	const size_t size = (o.bytes + gran - 1) / gran * gran;
	R.u64("size", size);
	CUmemGenericAllocationHandle h = 0;
	r = cuMemCreate(&h, size, &prop, 0);
	R.cu("cuMemCreate", r);
	CUdeviceptr va = 0;
	int fd = -1;
	unsigned long long gen = 1, updates = 0;
	if (r == CUDA_SUCCESS) {
		R.cu("cuMemAddressReserve", cuMemAddressReserve(&va, size, gran, 0, 0));
		R.cu("cuMemMap", cuMemMap(va, size, 0, h, 0));
		CUmemAccessDesc ad;
		memset(&ad, 0, sizeof(ad));
		ad.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
		ad.location.id = 0;
		ad.flags = CU_MEM_ACCESS_FLAGS_PROT_READWRITE;
		R.cu("cuMemSetAccess_device", cuMemSetAccess(va, size, &ad, 1));
		R.str("fill_mode", fill_names[o.fill_mode]);
		fill<<<64, 256>>>((unsigned long long *)va, size / 8, gen, o.fill_mode);
		R.str("fill_launch", cudaGetErrorName(cudaGetLastError()));
		R.str("fill_sync", cudaGetErrorName(cudaDeviceSynchronize()));
		r = cuMemExportToShareableHandle(
			&fd, h, CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR, 0);
		R.cu("cuMemExportToShareableHandle", r);
		if (r == CUDA_SUCCESS)
			describe_fd(R, fd, "p1");
	}
	// always tell process 2 something: fd (or /dev/null) and the size
	int sendfd = fd >= 0 ? fd : open("/dev/null", O_RDONLY);
	uint64_t sz = fd >= 0 ? size : 0;
	R.u64("fd_sent", send_fd(sock, sendfd, sz) == 0);
	// update loop: after process 2 says 'R' (ready), rewrite the buffer
	// with gen + 1 every update_us until it says 'D' (done) or the deadline
	const double t_dead = now_s() + o.seconds + 30;
	int b = 0;
	while (now_s() < t_dead) {
		b = recv_byte(sock, 1000);
		if (b == 'R' || b == 'D' || b < 0)
			break;
	}
	R.str("p2_first_msg", b == 'R' ? "R" : (b == 'D' ? "D" : (b < 0 ? "EOF" : "timeout")));
	double t_upd0 = now_s(), t_upd1 = t_upd0, fill_s = 0;
	if (b == 'R' && va) {
		cudaStream_t s;
		cudaStreamCreateWithFlags(&s, cudaStreamNonBlocking);
		while (now_s() < t_dead) {
			gen++;
			const double f0 = now_s();
			fill<<<64, 256, 0, s>>>((unsigned long long *)va, size / 8, gen,
						o.fill_mode);
			cudaStreamSynchronize(s);
			fill_s += now_s() - f0;
			updates++;
			t_upd1 = now_s();
			int m = recv_byte(sock, (int)(o.update_us / 1000 > 0 ? o.update_us / 1000 : 0));
			if (m == 'D' || m < 0) {
				b = m;
				break;
			}
		}
		R.str("update_err", cudaGetErrorName(cudaGetLastError()));
		cudaStreamDestroy(s);
	}
	R.u64("updates", updates);
	R.num("fill_us_mean", updates ? fill_s / updates * 1e6 : -1);
	R.u64("last_gen", gen);
	R.num("update_s", t_upd1 - t_upd0);
	R.str("p2_last_msg", b == 'D' ? "D" : (b < 0 ? "EOF" : "other"));
	// stopped: tell process 2 (it scans while this context is still alive
	// and holds the memory), then wait for 'F' before releasing
	if (b == 'D') {
		R.str("stop_sync", cudaGetErrorName(cudaDeviceSynchronize()));
		R.u64("stop_sent", write(sock, "S", 1) == 1);
		int f = 0;
		const double t_f = now_s() + 30;
		while (now_s() < t_f) {
			f = recv_byte(sock, 1000);
			if (f == 'F' || f < 0)
				break;
		}
		R.str("p2_scan_msg", f == 'F' ? "F" : (f < 0 ? "EOF" : "timeout"));
	}
	if (va) {
		R.cu("cuMemUnmap", cuMemUnmap(va, size));
		R.cu("cuMemAddressFree", cuMemAddressFree(va, size));
	}
	if (h)
		R.cu("cuMemRelease", cuMemRelease(h));
	if (fd >= 0)
		close(fd);
	if (sendfd != fd)
		close(sendfd);
	R.write(o.out);
	return 0;
}

// ------------------------------------------------------------ process 2
static int importer(int sock, const Opts &o)
{
	Rec R;
	R.role = "importer";
	alarm((unsigned)(o.seconds + 60));
	uint64_t size = 0;
	int fd = recv_fd(sock, &size);
	R.u64("fd_received", fd >= 0);
	R.u64("size", size);
	if (fd < 0 || size == 0) {
		R.write(o.out);
		return 1;
	}
	describe_fd(R, fd, "p2");
	// (iii-a) CPU mapping of the fd before any CUDA call
	void *cpu = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	R.u64("mmap_before_init_ok", cpu != MAP_FAILED);
	R.str("mmap_before_init_errno", cpu == MAP_FAILED ? strerror(errno) : "");
	int cpu_ok = 0;
	if (cpu != MAP_FAILED) {
		unsigned long long w[2] = { 0, 0 };
		int sig = guarded_read2((const volatile unsigned long long *)cpu,
					size / 8, w);
		R.u64("mmap_read_signal", sig);
		R.u64("mmap_word0", w[0]);
		R.u64("mmap_wordlast", w[1]);
		cpu_ok = (sig == 0);
		if (cpu_ok) {
			// static check: process 1 wrote generation 1 and synchronized
			R.u64("cpu_gen1_stale_plain",
			      cpu_scan((const volatile unsigned long long *)cpu,
				       size / 8, 1, 0));
			R.u64("cpu_gen1_stale_civac",
			      cpu_scan((const volatile unsigned long long *)cpu,
				       size / 8, 1, 1));
		}
	}
	R.cu("cuInit", cuInit(0));
	cudaFree(0);
	CUmemGenericAllocationHandle h = 0;
	CUresult r = cuMemImportFromShareableHandle(
		&h, (void *)(uintptr_t)fd, CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR);
	R.cu("cuMemImportFromShareableHandle", r);
	if (r != CUDA_SUCCESS) {
		if (write(sock, "D", 1) != 1)
			R.str("sock_write", "failed");
		R.write(o.out);
		return 1;
	}
	CUmemAllocationProp prop;
	memset(&prop, 0, sizeof(prop));
	R.cu("cuMemGetAllocationPropertiesFromHandle",
	     cuMemGetAllocationPropertiesFromHandle(&prop, h));
	R.u64("prop_type", prop.type);
	R.u64("prop_location_type", prop.location.type);
	R.u64("prop_location_id", (unsigned long long)prop.location.id);
	R.u64("prop_handle_types", prop.requestedHandleTypes);
	R.u64("prop_rdma", prop.allocFlags.gpuDirectRDMACapable);
	size_t gran = 0;
	CUmemAllocationProp gp;
	memset(&gp, 0, sizeof(gp));
	gp.type = CU_MEM_ALLOCATION_TYPE_PINNED;
	gp.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
	gp.location.id = 0;
	cuMemGetAllocationGranularity(&gran, &gp, CU_MEM_ALLOC_GRANULARITY_MINIMUM);
	CUdeviceptr va = 0;
	R.cu("cuMemAddressReserve", cuMemAddressReserve(&va, size, gran, 0, 0));
	R.cu("cuMemMap", cuMemMap(va, size, 0, h, 0));
	CUmemAccessDesc ad;
	memset(&ad, 0, sizeof(ad));
	ad.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
	ad.location.id = 0;
	ad.flags = CU_MEM_ACCESS_FLAGS_PROT_READWRITE;
	r = cuMemSetAccess(va, size, &ad, 1);
	R.cu("cuMemSetAccess_device", r);
	const size_t n = size / 8;
	unsigned long long *bad_d = nullptr, *smp_d = nullptr;
	cudaMalloc(&bad_d, 8);
	cudaMalloc(&smp_d, 16);
	// (i) kernel read of generation 1
	if (r == CUDA_SUCCESS) {
		cudaMemset(bad_d, 0, 8);
		check<<<64, 256>>>((const unsigned long long *)va, n, 1, bad_d);
		R.str("check_launch", cudaGetErrorName(cudaGetLastError()));
		R.str("check_sync", cudaGetErrorName(cudaDeviceSynchronize()));
		unsigned long long bad = ~0ull;
		cudaMemcpy(&bad, bad_d, 8, cudaMemcpyDeviceToHost);
		R.u64("kernel_bad_words_gen1", bad);
	}
	// (ii) DtoH into pageable memory and into registered host memory
	std::vector<unsigned long long> pg(n);
	r = cuMemcpyDtoH(pg.data(), va, size);
	R.cu("cuMemcpyDtoH_pageable", r);
	unsigned long long badh = 0;
	for (size_t i = 0; i < n; ++i)
		badh += pg[i] != ((1ull << 32) | (unsigned long long)(i & 0xffffffffu));
	R.u64("dtoh_bad_words_gen1", r == CUDA_SUCCESS ? badh : ~0ull);
	regbuf_t hb;
	int hb_ok = regbuf_create(&hb, size, "c") == 0;
	R.cu("cuMemHostRegister_dst", hb_ok ? CUDA_SUCCESS : hb.reg);
	CUstream s;
	cuStreamCreate(&s, CU_STREAM_NON_BLOCKING);
	if (hb_ok) {
		// bandwidth: imported memory vs a local cuMemAlloc buffer
		CUdeviceptr loc = 0;
		R.cu("cuMemAlloc_local", cuMemAlloc(&loc, size));
		const int N = 20;
		for (int pass = 0; pass < 2; ++pass) {
			CUdeviceptr src = pass == 0 ? va : loc;
			if (!src)
				continue;
			CK(cuMemcpyDtoHAsync(hb.p, src, size, s));
			CK(cuStreamSynchronize(s));
			std::vector<double> lat;
			const double t0 = now_s();
			for (int i = 0; i < N; ++i) {
				const double a = now_s();
				CK(cuMemcpyDtoHAsync(hb.p, src, size, s));
				CK(cuStreamSynchronize(s));
				lat.push_back((now_s() - a) * 1e6);
			}
			const double el = now_s() - t0;
			std::sort(lat.begin(), lat.end());
			R.num(pass == 0 ? "dtoh_imported_GBps" : "dtoh_local_GBps",
			      (double)size * N / el / 1e9);
			R.num(pass == 0 ? "dtoh_imported_lat_us_p50" : "dtoh_local_lat_us_p50",
			      lat[N / 2]);
		}
		if (loc)
			cuMemFree(loc);
	}
	// (iii-b) CPU access through the driver
	CUmemAccessDesc hd;
	memset(&hd, 0, sizeof(hd));
	hd.location.type = CU_MEM_LOCATION_TYPE_HOST;
	hd.location.id = 0;
	hd.flags = CU_MEM_ACCESS_FLAGS_PROT_READ;
	CUresult rh = cuMemSetAccess(va, size, &hd, 1);
	R.cu("cuMemSetAccess_host", rh);
	hd.location.type = CU_MEM_LOCATION_TYPE_HOST_NUMA;
	CUresult rn = cuMemSetAccess(va, size, &hd, 1);
	R.cu("cuMemSetAccess_host_numa", rn);
	if (rh == CUDA_SUCCESS || rn == CUDA_SUCCESS) {
		unsigned long long w[2] = { 0, 0 };
		int sig = guarded_read2((const volatile unsigned long long *)va, n, w);
		R.u64("va_cpu_read_signal", sig);
		R.u64("va_cpu_word0", w[0]);
	}
	void *hp = nullptr;
	R.cu("cuPointerGetAttribute_HOST_POINTER",
	     cuPointerGetAttribute(&hp, CU_POINTER_ATTRIBUTE_HOST_POINTER, va));
	// (iii-d) CPU mapping of the fd after import
	void *cpu2 = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	R.u64("mmap_after_import_ok", cpu2 != MAP_FAILED);
	R.str("mmap_after_import_errno", cpu2 == MAP_FAILED ? strerror(errno) : "");
	if (!cpu_ok && cpu2 != MAP_FAILED) {
		unsigned long long w[2] = { 0, 0 };
		int sig = guarded_read2((const volatile unsigned long long *)cpu2, n, w);
		R.u64("mmap2_read_signal", sig);
		R.u64("mmap2_word0", w[0]);
		if (sig == 0) {
			cpu = cpu2;
			cpu_ok = 1;
		}
	}
	// live phase: process 1 rewrites the buffer with gen + 1 every update_us;
	// sample it every ~1 ms through DtoH, a kernel, and the CPU mapping
	if (write(sock, "R", 1) != 1)
		R.str("sock_write", "failed");
	unsigned long long last_d = 0, last_k = 0, last_c = 0;
	unsigned long long distinct_d = 0, distinct_k = 0, distinct_c = 0;
	unsigned long long back_d = 0, back_k = 0, back_c = 0;
	unsigned long long mixed_d = 0, mixed_k = 0, mixed_c = 0;
	unsigned long long samples = 0, errs = 0, cpu_faults = 0;
	unsigned long long cpu_vs_dtoh_older = 0;
	const double t0 = now_s();
	while (now_s() - t0 < o.seconds && samples < 100000) {
		samples++;
		unsigned long long w[2] = { 0, 0 }, k[2] = { 0, 0 };
		if (cuMemcpyDtoH(&w[0], va, 8) != CUDA_SUCCESS ||
		    cuMemcpyDtoH(&w[1], va + size - 8, 8) != CUDA_SUCCESS)
			errs++;
		const unsigned long long gd = w[0] >> 32;
		if (gd != last_d) {
			distinct_d++;
			if (gd < last_d)
				back_d++;
			last_d = gd;
		}
		if ((w[0] >> 32) != (w[1] >> 32))
			mixed_d++;
		sample<<<1, 1>>>((const volatile unsigned long long *)va, n, smp_d);
		if (cudaMemcpy(k, smp_d, 16, cudaMemcpyDeviceToHost) != cudaSuccess)
			errs++;
		const unsigned long long gk = k[0] >> 32;
		if (gk != last_k) {
			distinct_k++;
			if (gk < last_k)
				back_k++;
			last_k = gk;
		}
		if ((k[0] >> 32) != (k[1] >> 32))
			mixed_k++;
		if (cpu_ok) {
			unsigned long long c[2] = { 0, 0 };
			// read after the DtoH sample: a CPU value older than the
			// DtoH value means the CPU view is stale
			if (guarded_read2((const volatile unsigned long long *)cpu, n, c)) {
				cpu_faults++;
			} else {
				const unsigned long long gc = c[0] >> 32;
				if (gc != last_c) {
					distinct_c++;
					if (gc < last_c)
						back_c++;
					last_c = gc;
				}
				if ((c[0] >> 32) != (c[1] >> 32))
					mixed_c++;
				if (gc < gd)
					cpu_vs_dtoh_older++;
			}
		}
		usleep(1000);
	}
	if (write(sock, "D", 1) != 1)
		R.str("sock_write", "failed");
	R.u64("live_samples", samples);
	R.u64("live_errors", errs);
	R.u64("dtoh_distinct_gens", distinct_d);
	R.u64("dtoh_backwards", back_d);
	R.u64("dtoh_mixed", mixed_d);
	R.u64("dtoh_last_gen", last_d);
	R.u64("kernel_distinct_gens", distinct_k);
	R.u64("kernel_backwards", back_k);
	R.u64("kernel_mixed", mixed_k);
	R.u64("kernel_last_gen", last_k);
	R.u64("cpu_map_used", cpu_ok);
	R.u64("cpu_distinct_gens", distinct_c);
	R.u64("cpu_backwards", back_c);
	R.u64("cpu_mixed", mixed_c);
	R.u64("cpu_last_gen", last_c);
	R.u64("cpu_faults", cpu_faults);
	R.u64("cpu_older_than_dtoh", cpu_vs_dtoh_older);
	// quiescent phase: process 1 stopped writing and synchronized ('S'),
	// and still holds the memory until this process says 'F'
	int sm = 0;
	for (int i = 0; i < 100; ++i) {
		sm = recv_byte(sock, 100);
		if (sm == 'S' || sm < 0)
			break;
	}
	R.str("p1_stop_msg", sm == 'S' ? "S" : (sm < 0 ? "EOF" : "timeout"));
	unsigned long long fin[2] = { 0, 0 };
	cuMemcpyDtoH(fin, va, 8);
	const unsigned long long G = fin[0] >> 32;
	R.u64("final_dtoh_gen", G);
	{
		CUresult rf = cuMemcpyDtoH(pg.data(), va, size);
		unsigned long long bad = 0;
		for (size_t i = 0; i < n; ++i)
			bad += pg[i] != ((G << 32) | (unsigned long long)(i & 0xffffffffu));
		R.u64("final_dtoh_stale", rf == CUDA_SUCCESS ? bad : ~0ull);
	}
	if (cpu_ok) {
		// how many CPU-mapped words still differ from the final
		// generation, 0, 10, 100, 1000 ms after process 1 stopped
		const int delays[] = { 0, 10, 100, 1000 };
		double tq = now_s();
		for (int d : delays) {
			while ((now_s() - tq) * 1e3 < d)
				usleep(1000);
			char k1[64], k2[64];
			snprintf(k1, sizeof(k1), "cpu_final_stale_plain_%dms", d);
			snprintf(k2, sizeof(k2), "cpu_final_stale_civac_%dms", d);
			R.u64(k1, cpu_scan((const volatile unsigned long long *)cpu, n, G, 0));
			R.u64(k2, cpu_scan((const volatile unsigned long long *)cpu, n, G, 1));
		}
	}
	if (write(sock, "F", 1) != 1)
		R.str("sock_write", "failed");
	// after process 1 released its mapping and exited (EOF, bounded)
	int eof = 0;
	for (int i = 0; i < 100; ++i) {
		if (recv_byte(sock, 100) < 0) {
			eof = 1;
			break;
		}
	}
	R.u64("p1_eof_seen", eof);
	if (cpu_ok && eof)
		R.u64("cpu_stale_after_p1_exit_plain",
		      cpu_scan((const volatile unsigned long long *)cpu, n, G, 0));
	if (hb_ok)
		regbuf_destroy(&hb);
	R.cu("cuMemUnmap", cuMemUnmap(va, size));
	R.cu("cuMemAddressFree", cuMemAddressFree(va, size));
	R.cu("cuMemRelease", cuMemRelease(h));
	close(fd);
	R.write(o.out);
	return 0;
}

int main(int argc, char **argv)
{
	Opts o;
	for (int i = 1; i + 1 < argc; i += 2) {
		if (!strcmp(argv[i], "--bytes"))
			o.bytes = strtoull(argv[i + 1], nullptr, 0);
		else if (!strcmp(argv[i], "--seconds"))
			o.seconds = atof(argv[i + 1]);
		else if (!strcmp(argv[i], "--update-us"))
			o.update_us = atol(argv[i + 1]);
		else if (!strcmp(argv[i], "--rdma"))
			o.rdma = atoi(argv[i + 1]);
		else if (!strcmp(argv[i], "--fill")) {
			o.fill_mode = !strcmp(argv[i + 1], "wt") ? 1 :
				      !strcmp(argv[i + 1], "sysfence") ? 2 : 0;
		}
		else if (!strcmp(argv[i], "--out"))
			o.out = argv[i + 1];
		else {
			fprintf(stderr, "bad option %s\n", argv[i]);
			return 1;
		}
	}
	int sv[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
		perror("socketpair");
		return 1;
	}
	// fork before any CUDA call
	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		return 1;
	}
	if (pid == 0) {
		close(sv[0]);
		int rc = importer(sv[1], o);
		close(sv[1]);
		_exit(rc);
	}
	close(sv[1]);
	int rc = exporter(sv[0], o);
	close(sv[0]);
	int status = 0;
	// the importer has its own alarm(); this wait is bounded by it
	waitpid(pid, &status, 0);
	FILE *f = fopen(o.out, "a");
	if (f) {
		fprintf(f,
			"{\"test\":\"vmmexport\",\"role\":\"parent\",\"importer_exit\":%d,"
			"\"importer_signal\":%d,\"exporter_rc\":%d}\n",
			WIFEXITED(status) ? WEXITSTATUS(status) : -1,
			WIFSIGNALED(status) ? WTERMSIG(status) : 0, rc);
		fclose(f);
	}
	return 0;
}

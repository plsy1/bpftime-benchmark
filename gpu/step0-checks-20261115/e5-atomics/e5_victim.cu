// e5_victim [blocks=48] [threads_per_block=256] [launches=10] [--report-mem] [--alloc-mib N]
// The kernel the e5 programs probe (kretprobe/e5step0_atomics_kernel): every
// thread writes one word and returns. Launches are serialized (one stream,
// synchronized after each), so the probe's per-thread seen[] word is private.
// --report-mem (used by e6 part c): print, at main entry and at the end, the
//   time from process start to main (the agent registers the segment before
//   main), MemAvailable/Shmem, VmRSS/VmPin/VmLck, and the Size/Rss/Locked of
//   the bpftime segment mapping (/dev/shm/$BPFTIME_GLOBAL_SHM_NAME).
// --alloc-mib N: after the probed launches, cudaMalloc N MiB, memset it on the
//   device and read a sample back ("the GPU workload still works").
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <unistd.h>
#include <cuda_runtime.h>

extern "C" __global__ void e5step0_atomics_kernel(unsigned long long *out)
{
	unsigned g = blockIdx.x * blockDim.x + threadIdx.x;
	out[g] = (unsigned long long)g * 2ull + 1ull;
}

static long long meminfo_kb(const char *key)
{
	FILE *f = fopen("/proc/meminfo", "r");
	if (!f)
		return -1;
	char line[256];
	long long v = -1;
	size_t kl = strlen(key);
	while (fgets(line, sizeof(line), f))
		if (!strncmp(line, key, kl) && line[kl] == ':') {
			v = atoll(line + kl + 1);
			break;
		}
	fclose(f);
	return v;
}

static long long status_kb(const char *key)
{
	FILE *f = fopen("/proc/self/status", "r");
	if (!f)
		return -1;
	char line[256];
	long long v = -1;
	size_t kl = strlen(key);
	while (fgets(line, sizeof(line), f))
		if (!strncmp(line, key, kl) && line[kl] == ':') {
			v = atoll(line + kl + 1);
			break;
		}
	fclose(f);
	return v;
}

// Size/Rss/Locked (kB) of the mapping whose path ends with the shm name.
static void segment_smaps(long long *size, long long *rss, long long *locked)
{
	*size = *rss = *locked = -1;
	const char *name = getenv("BPFTIME_GLOBAL_SHM_NAME");
	if (!name)
		return;
	std::string want = std::string("/dev/shm/") + name;
	FILE *f = fopen("/proc/self/smaps", "r");
	if (!f)
		return;
	char line[1024];
	bool in = false;
	while (fgets(line, sizeof(line), f)) {
		// a mapping header ("addr-addr perms off dev inode path") starts
		// with a hex digit; field lines ("Rss: ...") with a capital
		bool header = (line[0] >= '0' && line[0] <= '9') ||
			      (line[0] >= 'a' && line[0] <= 'f');
		if (header) {
			size_t l = strlen(line);
			while (l && (line[l - 1] == '\n' || line[l - 1] == ' '))
				line[--l] = 0;
			in = l >= want.size() &&
			     !strcmp(line + l - want.size(), want.c_str());
			continue;
		}
		if (!in)
			continue;
		long long v;
		if (sscanf(line, "Size: %lld", &v) == 1)
			*size = (*size < 0 ? 0 : *size) + v;
		else if (sscanf(line, "Rss: %lld", &v) == 1)
			*rss = (*rss < 0 ? 0 : *rss) + v;
		else if (sscanf(line, "Locked: %lld", &v) == 1)
			*locked = (*locked < 0 ? 0 : *locked) + v;
	}
	fclose(f);
}

// ms from process start (from /proc/self/stat starttime, 10 ms ticks) to now.
static double ms_since_start(void)
{
	FILE *f = fopen("/proc/self/stat", "r");
	if (!f)
		return -1;
	char buf[2048];
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = 0;
	char *p = strrchr(buf, ')');
	if (!p)
		return -1;
	// fields after the comm: state is field 3; starttime is field 22
	int field = 2;
	unsigned long long start = 0;
	for (char *t = strtok(p + 1, " "); t; t = strtok(NULL, " ")) {
		field++;
		if (field == 22) {
			start = strtoull(t, NULL, 10);
			break;
		}
	}
	long hz = sysconf(_SC_CLK_TCK);
	struct timespec ts;
	clock_gettime(CLOCK_BOOTTIME, &ts);
	double now_ms = ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
	return now_ms - (double)start * 1e3 / (double)hz;
}

static void report_mem(const char *when)
{
	long long sz, rss, lck;
	segment_smaps(&sz, &rss, &lck);
	printf("E5MEM {\"when\":\"%s\",\"ms_since_start\":%.1f,"
	       "\"MemAvailable_kB\":%lld,\"Shmem_kB\":%lld,\"Mlocked_kB\":%lld,"
	       "\"VmRSS_kB\":%lld,\"VmPin_kB\":%lld,\"VmLck_kB\":%lld,"
	       "\"seg_Size_kB\":%lld,\"seg_Rss_kB\":%lld,\"seg_Locked_kB\":%lld}\n",
	       when, ms_since_start(), meminfo_kb("MemAvailable"),
	       meminfo_kb("Shmem"), meminfo_kb("Mlocked"), status_kb("VmRSS"),
	       status_kb("VmPin"), status_kb("VmLck"), sz, rss, lck);
	fflush(stdout);
}

#define CK(x)                                                                  \
	do {                                                                   \
		cudaError_t e_ = (x);                                          \
		if (e_ != cudaSuccess) {                                       \
			printf("E5VICTIM {\"error\":\"%s\",\"at\":\"%s\"}\n",  \
			       cudaGetErrorString(e_), #x);                    \
			return 1;                                              \
		}                                                              \
	} while (0)

int main(int argc, char **argv)
{
	bool rep = false;
	long alloc_mib = 0;
	std::vector<long> pos;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--report-mem"))
			rep = true;
		else if (!strcmp(argv[i], "--alloc-mib") && i + 1 < argc)
			alloc_mib = atol(argv[++i]);
		else
			pos.push_back(atol(argv[i]));
	}
	if (rep)
		report_mem("main");
	unsigned blocks = pos.size() > 0 ? pos[0] : 48;
	unsigned tpb = pos.size() > 1 ? pos[1] : 256;
	unsigned launches = pos.size() > 2 ? pos[2] : 10;
	size_t n = (size_t)blocks * tpb;
	unsigned long long *d = nullptr;
	CK(cudaMalloc(&d, n * sizeof(*d)));
	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (unsigned l = 0; l < launches; l++) {
		CK(cudaMemset(d, 0, n * sizeof(*d)));
		e5step0_atomics_kernel<<<blocks, tpb>>>(d);
		CK(cudaGetLastError());
		CK(cudaDeviceSynchronize());
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	std::vector<unsigned long long> h(n);
	CK(cudaMemcpy(h.data(), d, n * sizeof(*d), cudaMemcpyDeviceToHost));
	size_t bad = 0;
	for (size_t i = 0; i < n; i++)
		if (h[i] != (unsigned long long)i * 2ull + 1ull)
			bad++;
	CK(cudaFree(d));
	long alloc_ok = -1;
	if (alloc_mib > 0) {
		size_t bytes = (size_t)alloc_mib << 20;
		void *p = nullptr;
		alloc_ok = 0;
		if (cudaMalloc(&p, bytes) == cudaSuccess) {
			unsigned char s[4] = { 0, 0, 0, 0 };
			if (cudaMemset(p, 0x5a, bytes) == cudaSuccess &&
			    cudaDeviceSynchronize() == cudaSuccess &&
			    cudaMemcpy(s, (char *)p + bytes - 4, 4,
				       cudaMemcpyDeviceToHost) == cudaSuccess &&
			    s[0] == 0x5a && s[3] == 0x5a)
				alloc_ok = 1;
			cudaFree(p);
		}
	}
	printf("E5VICTIM {\"blocks\":%u,\"threads_per_block\":%u,\"launches\":%u,"
	       "\"threads\":%zu,\"output_mismatches\":%zu,\"launch_ms\":%.3f,"
	       "\"alloc_mib\":%ld,\"alloc_ok\":%ld}\n",
	       blocks, tpb, launches, n, bad,
	       (t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) / 1e6,
	       alloc_mib, alloc_ok);
	if (rep)
		report_mem("end");
	return bad ? 1 : 0;
}

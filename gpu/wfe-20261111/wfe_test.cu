// Can a GPU write to host memory wake an aarch64 core waiting in WFE?
// The GPU plays a host-call client: every round it waits DELAY us (globaltimer),
// writes `flag` = round, and times (globaltimer) until the CPU answers by writing
// `ack` = round. The CPU thread waits for `flag` with one of three policies:
//   spin  : load-acquire in a loop
//   sleep : bpftime's watcher (200 us polling after an answer, then sleeps of
//           50 us doubling to 1 ms)
//   wfe   : load-exclusive, WFE until the line is written or another event
//           (Linux's __cmpwait pattern); counts its wake-ups
// Memory: a page of malloc'd memory registered with cuMemHostRegister
// (DEVICEMAP | PORTABLE), as bpftime registers its segment.
// Output per policy and delay: GPU-timed round trip (p50/p90/max, us), CPU time
// of the waiting thread over the run (% of one core), WFE wake-ups per round.
#include <cuda.h>
#include <cuda_runtime.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

struct shared_t {
	alignas(64) volatile uint32_t flag;
	alignas(64) volatile uint32_t ack;
	alignas(64) volatile uint32_t stop;
};

__device__ __forceinline__ uint64_t gtime()
{
	uint64_t t;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	return t;
}

__global__ void client(shared_t *s, int rounds, uint64_t delay_ns,
		       unsigned long long *rtt)
{
	for (int r = 1; r <= rounds; r++) {
		uint64_t t0 = gtime();
		while (gtime() - t0 < delay_ns)
			;
		uint64_t w = gtime();
		s->flag = (uint32_t)r;
		__threadfence_system();
		while (s->ack != (uint32_t)r)
			;
		rtt[r - 1] = gtime() - w;
	}
	s->stop = 1;
	__threadfence_system();
}

static inline uint32_t load_acquire(volatile uint32_t *p)
{
	return __atomic_load_n(p, __ATOMIC_ACQUIRE);
}

// Linux arm64 __cmpwait: returns after an event if *p may differ from old
static inline void cmpwait(volatile uint32_t *p, uint32_t old)
{
	uint32_t tmp;
	asm volatile("sevl\n"
		     "wfe\n"
		     "ldxr %w[tmp], %[v]\n"
		     "eor %w[tmp], %w[tmp], %w[old]\n"
		     "cbnz %w[tmp], 1f\n"
		     "wfe\n"
		     "1:"
		     : [tmp] "=&r"(tmp), [v] "+Q"(*(volatile uint32_t *)p)
		     : [old] "r"(old)
		     : "memory");
}

static double thread_cpu_s()
{
	timespec ts;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
	return ts.tv_sec + ts.tv_nsec * 1e-9;
}

struct result {
	double cpu_s = 0, wall_s = 0;
	unsigned long long wakeups = 0;
};

static void server(shared_t *s, const std::string &policy, result *out)
{
	const double c0 = thread_cpu_s();
	auto w0 = std::chrono::steady_clock::now();
	uint32_t seen = 0;
	auto last = std::chrono::steady_clock::now();
	auto sleep = std::chrono::microseconds(50);
	while (!load_acquire(&s->stop)) {
		uint32_t f = load_acquire(&s->flag);
		if (f != seen) {
			seen = f;
			__atomic_store_n(&s->ack, f, __ATOMIC_RELEASE);
			last = std::chrono::steady_clock::now();
			sleep = std::chrono::microseconds(50);
			continue;
		}
		if (policy == "spin")
			continue;
		if (policy == "wfe") {
			cmpwait(&s->flag, f);
			out->wakeups++;
			continue;
		}
		// bpftime watcher
		if (std::chrono::steady_clock::now() - last <
		    std::chrono::microseconds(200)) {
			asm volatile("yield" ::: "memory");
			continue;
		}
		std::this_thread::sleep_for(sleep);
		sleep = std::min<std::chrono::microseconds>(
			sleep * 2, std::chrono::microseconds(1000));
	}
	out->cpu_s = thread_cpu_s() - c0;
	out->wall_s = std::chrono::duration<double>(
			      std::chrono::steady_clock::now() - w0)
			      .count();
}

int main(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "usage: %s spin|sleep|wfe DELAY_US ROUNDS\n", argv[0]);
		return 2;
	}
	std::string policy = argv[1];
	const uint64_t delay_ns = strtoull(argv[2], 0, 10) * 1000;
	const int rounds = atoi(argv[3]);
	cuInit(0);
	cudaSetDevice(0);
	cudaFree(0);
	void *page = nullptr;
	if (posix_memalign(&page, 4096, 4096))
		return 1;
	memset(page, 0, 4096);
	if (cuMemHostRegister(page, 4096, CU_MEMHOSTREGISTER_DEVICEMAP |
						  CU_MEMHOSTREGISTER_PORTABLE) !=
	    CUDA_SUCCESS) {
		fprintf(stderr, "cuMemHostRegister failed\n");
		return 1;
	}
	CUdeviceptr dev;
	cuMemHostGetDevicePointer(&dev, page, 0);
	shared_t *s = (shared_t *)page;
	unsigned long long *rtt;
	cudaMalloc(&rtt, rounds * sizeof(*rtt));
	result res;
	std::thread t(server, s, policy, &res);
	client<<<1, 1>>>((shared_t *)dev, rounds, delay_ns, rtt);
	cudaError_t err = cudaDeviceSynchronize();
	t.join();
	if (err != cudaSuccess) {
		fprintf(stderr, "kernel: %s\n", cudaGetErrorString(err));
		return 1;
	}
	std::vector<unsigned long long> v(rounds);
	cudaMemcpy(v.data(), rtt, rounds * sizeof(*rtt), cudaMemcpyDeviceToHost);
	std::sort(v.begin(), v.end());
	auto q = [&](double p) { return v[std::min<size_t>(v.size() - 1, (size_t)(p * v.size()))] / 1000.0; };
	printf("policy=%s delay_us=%llu rounds=%d rtt_us p50=%.2f p90=%.2f max=%.2f cpu=%.1f%% wakeups_per_round=%.2f\n",
	       policy.c_str(), (unsigned long long)(delay_ns / 1000), rounds,
	       q(0.5), q(0.9), v.back() / 1000.0,
	       100.0 * res.cpu_s / res.wall_s,
	       (double)res.wakeups / rounds);
	return 0;
}

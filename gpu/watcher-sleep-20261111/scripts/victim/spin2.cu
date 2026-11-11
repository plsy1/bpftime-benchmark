// Victim for the short-sleep test: spin2 LAUNCHES SPIN_US JITTER_US THREADS
// L launches back to back of a 1 x THREADS kernel spinning on %globaltimer for
// SPIN_US plus a per-launch extra of 0..JITTER_US-1 us (a fixed LCG sequence,
// the same in every run), so a probe's call at the kernel's exit lands at a
// random phase of any sleep schedule. Every lane spins the same time.
// Prints CUDA's device flags, the timed loop's window (CLOCK_MONOTONIC and
// CLOCK_REALTIME, ns), per-launch mean/median/p90/max (us), the mean spin asked
// (us), and the process CPU seconds in the window (getrusage, all threads).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <sys/resource.h>
#include <cuda_runtime.h>

__global__ void spinKernel(unsigned long long ns, unsigned long long *out)
{
	unsigned long long t0, t;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t0));
	do {
		asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	} while (t - t0 < ns);
	if (threadIdx.x == 0)
		out[0] = t - t0;
}

static long long clk(clockid_t c)
{
	timespec ts;
	clock_gettime(c, &ts);
	return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static double cpu_s()
{
	rusage ru;
	getrusage(RUSAGE_SELF, &ru);
	return ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1e6 +
	       ru.ru_stime.tv_sec + ru.ru_stime.tv_usec / 1e6;
}

int main(int argc, char **argv)
{
	if (argc < 5) {
		fprintf(stderr, "usage: spin2 LAUNCHES SPIN_US JITTER_US THREADS\n");
		return 2;
	}
	int launches = atoi(argv[1]);
	unsigned long long spin_ns = strtoull(argv[2], nullptr, 10) * 1000ULL;
	unsigned jitter_us = (unsigned)atoi(argv[3]);
	int threads = atoi(argv[4]);
	std::vector<unsigned long long> ns(launches);
	unsigned long long x = 12345, sum_ns = 0;
	for (int i = 0; i < launches; i++) {
		x = x * 6364136223846793005ULL + 1442695040888963407ULL;
		unsigned extra = jitter_us ? (unsigned)((x >> 33) % jitter_us) : 0;
		ns[i] = spin_ns + extra * 1000ULL;
		sum_ns += ns[i];
	}
	unsigned long long *d_out;
	if (cudaMalloc(&d_out, sizeof(*d_out)) != cudaSuccess) {
		fprintf(stderr, "cudaMalloc failed\n");
		return 1;
	}
	unsigned int flags = 0;
	cudaGetDeviceFlags(&flags);
	printf("spin device_flags=0x%x\n", flags);
	spinKernel<<<1, threads>>>(1000, d_out); // warm-up
	if (cudaDeviceSynchronize() != cudaSuccess) {
		fprintf(stderr, "warm-up failed\n");
		return 1;
	}
	std::vector<double> t(launches);
	double c0 = cpu_s();
	long long s = clk(CLOCK_MONOTONIC), rs = clk(CLOCK_REALTIME);
	for (int i = 0; i < launches; i++) {
		long long a = clk(CLOCK_MONOTONIC);
		spinKernel<<<1, threads>>>(ns[i], d_out);
		cudaError_t e = cudaDeviceSynchronize();
		if (e != cudaSuccess) {
			fprintf(stderr, "launch %d: %s\n", i, cudaGetErrorString(e));
			return 1;
		}
		t[i] = (clk(CLOCK_MONOTONIC) - a) / 1000.0;
	}
	long long e = clk(CLOCK_MONOTONIC), re = clk(CLOCK_REALTIME);
	double c1 = cpu_s();
	double sum = 0;
	for (double v : t)
		sum += v;
	std::vector<double> sorted = t;
	std::sort(sorted.begin(), sorted.end());
	printf("spin window_ns=%lld %lld\n", s, e);
	printf("spin realtime_ns=%lld %lld\n", rs, re);
	printf("spin launches=%d threads=%d spin_us=%llu jitter_us=%u mean_spin_asked_us=%.1f\n",
	       launches, threads, spin_ns / 1000, jitter_us,
	       sum_ns / 1000.0 / launches);
	printf("spin per_launch_us mean=%.1f median=%.1f p90=%.1f max=%.1f\n",
	       sum / launches, sorted[launches / 2],
	       sorted[(size_t)(launches * 0.9)], sorted.back());
	printf("spin cpu_s=%.3f wall_s=%.3f\n", c1 - c0, (e - s) / 1e9);
	return 0;
}

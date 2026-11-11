// Victim for the WFE-under-load test: L launches back to back of a 1x1 kernel
// that spins on %globaltimer for D us (launch + cudaDeviceSynchronize each).
// spin L D
// Prints CUDA's device flags, CLOCK_MONOTONIC (ns) at the timed loop's start
// and end, per-launch mean/median (us), and the process CPU seconds
// (getrusage, all threads) spent during the timed loop.
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
	out[0] = t - t0;
}

static long long mono_ns()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
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
	if (argc < 3) {
		fprintf(stderr, "usage: spin LAUNCHES SPIN_US\n");
		return 2;
	}
	int launches = atoi(argv[1]);
	unsigned long long ns = strtoull(argv[2], nullptr, 10) * 1000ULL;
	unsigned long long *d_out;
	if (cudaMalloc(&d_out, sizeof(*d_out)) != cudaSuccess) {
		fprintf(stderr, "cudaMalloc failed\n");
		return 1;
	}
	unsigned int flags = 0;
	cudaGetDeviceFlags(&flags);
	printf("spin device_flags=0x%x\n", flags);
	// warm-up
	spinKernel<<<1, 1>>>(1000, d_out);
	if (cudaDeviceSynchronize() != cudaSuccess) {
		fprintf(stderr, "warm-up failed\n");
		return 1;
	}
	std::vector<double> t(launches);
	double c0 = cpu_s();
	long long s = mono_ns();
	for (int i = 0; i < launches; i++) {
		long long a = mono_ns();
		spinKernel<<<1, 1>>>(ns, d_out);
		cudaError_t e = cudaDeviceSynchronize();
		if (e != cudaSuccess) {
			fprintf(stderr, "launch %d: %s\n", i, cudaGetErrorString(e));
			return 1;
		}
		t[i] = (mono_ns() - a) / 1000.0;
	}
	long long e = mono_ns();
	double c1 = cpu_s();
	unsigned long long last = 0;
	cudaMemcpy(&last, d_out, sizeof(last), cudaMemcpyDeviceToHost);
	double sum = 0;
	for (double v : t)
		sum += v;
	std::vector<double> sorted = t;
	std::sort(sorted.begin(), sorted.end());
	printf("spin window_ns=%lld %lld\n", s, e);
	printf("spin launches=%d spin_us=%llu last_spin_ns=%llu\n", launches,
	       ns / 1000, last);
	printf("spin per_launch_us mean=%.1f median=%.1f p90=%.1f max=%.1f\n",
	       sum / launches, sorted[launches / 2],
	       sorted[(size_t)(launches * 0.9)], sorted.back());
	printf("spin cpu_s=%.3f wall_s=%.3f\n", c1 - c0, (e - s) / 1e9);
	return 0;
}

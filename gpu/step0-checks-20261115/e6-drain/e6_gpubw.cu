// e6_gpubw [seconds=40] [mib_per_buffer=256] [kernels_per_batch=8]
// The GPU workload of check 6 part (b): a device-memory copy kernel (float4,
// grid-stride) between two cudaMalloc buffers, timed with CUDA events in
// batches; each batch also carries its CLOCK_MONOTONIC bounds so that it can
// be matched with the drain phases logged by e6_drain (same clock, other
// process). Runs as a plain process (no bpftime). At most 100,000 batches.
// Output: one JSON line per batch "BW {...}" (also to $E6_BW_OUT if set) and
// a final "BWSUM {...}".
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cuda_runtime.h>

__global__ void e6_copy(const float4 *__restrict__ a, float4 *__restrict__ b, size_t n)
{
	for (size_t i = blockIdx.x * (size_t)blockDim.x + threadIdx.x; i < n;
	     i += (size_t)gridDim.x * blockDim.x)
		b[i] = a[i];
}

static uint64_t now_ns()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

#define CK(x)                                                                  \
	do {                                                                   \
		cudaError_t e_ = (x);                                          \
		if (e_ != cudaSuccess) {                                       \
			printf("BWSUM {\"error\":\"%s\",\"at\":\"%s\"}\n",     \
			       cudaGetErrorString(e_), #x);                    \
			return 1;                                              \
		}                                                              \
	} while (0)

int main(int argc, char **argv)
{
	double secs = argc > 1 ? atof(argv[1]) : 40;
	size_t mib = argc > 2 ? strtoull(argv[2], nullptr, 10) : 256;
	int per = argc > 3 ? atoi(argv[3]) : 8;
	size_t bytes = mib << 20, n = bytes / sizeof(float4);
	FILE *o = getenv("E6_BW_OUT") ? fopen(getenv("E6_BW_OUT"), "a") : nullptr;
	float4 *a, *b;
	CK(cudaMalloc(&a, bytes));
	CK(cudaMalloc(&b, bytes));
	CK(cudaMemset(a, 1, bytes));
	CK(cudaMemset(b, 0, bytes));
	cudaEvent_t e0, e1;
	CK(cudaEventCreate(&e0));
	CK(cudaEventCreate(&e1));
	int blocks = 8 * 6 * 4, tpb = 256; // 8 SMs x 6 resident 256-thread blocks x 4
	e6_copy<<<blocks, tpb>>>(a, b, n); // warm-up
	CK(cudaDeviceSynchronize());
	if (getenv("E6_GO_FILE")) { // tell the drain loader that the copy loop starts now
		FILE *g = fopen(getenv("E6_GO_FILE"), "w");
		if (g)
			fclose(g);
	}
	uint64_t start = now_ns(), batches = 0;
	double sum_ms = 0;
	while ((now_ns() - start) / 1e9 < secs && batches < 100000) {
		uint64_t t0 = now_ns();
		CK(cudaEventRecord(e0));
		for (int i = 0; i < per; i++)
			e6_copy<<<blocks, tpb>>>(a, b, n);
		CK(cudaEventRecord(e1));
		CK(cudaEventSynchronize(e1));
		uint64_t t1 = now_ns();
		float ms = 0;
		CK(cudaEventElapsedTime(&ms, e0, e1));
		CK(cudaGetLastError());
		double gbs = 2.0 * bytes * per / (ms / 1e3) / 1e9; // read + write
		char line[256];
		snprintf(line, sizeof(line),
			 "{\"batch\":%llu,\"t0_ns\":%llu,\"t1_ns\":%llu,\"gpu_ms\":%.4f,\"gb_per_s\":%.3f}",
			 (unsigned long long)batches, (unsigned long long)t0,
			 (unsigned long long)t1, ms, gbs);
		printf("BW %s\n", line);
		if (o)
			fprintf(o, "%s\n", line);
		sum_ms += ms;
		batches++;
	}
	printf("BWSUM {\"batches\":%llu,\"kernels_per_batch\":%d,\"mib_per_buffer\":%zu,"
	       "\"mean_gb_per_s\":%.3f,\"wall_s\":%.3f}\n",
	       (unsigned long long)batches, per, mib,
	       batches ? 2.0 * bytes * per * batches / (sum_ms / 1e3) / 1e9 : 0.0,
	       (now_ns() - start) / 1e9);
	if (o)
		fclose(o);
	return 0;
}

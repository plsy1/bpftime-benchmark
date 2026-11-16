// pt_exitfast: launches kernels and returns from main without synchronizing,
// with the last kernel still running (each thread spins SPIN_US): does the
// agent's final delivery (atexit) see every thread's exit?
// usage: pt_exitfast LAUNCHES SPIN_US
#include <cstdio>
#include <cstdlib>
#include <cuda_runtime.h>

extern "C" __global__ void pt_kslow(unsigned *out, unsigned spin_us)
{
	unsigned long long t0, t;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t0));
	do {
		__nanosleep(1000);
		asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	} while (t - t0 < 1000ull * spin_us);
	out[blockIdx.x * blockDim.x + threadIdx.x] = (unsigned)t;
}

int main(int argc, char **argv)
{
	int launches = argc > 1 ? atoi(argv[1]) : 3;
	unsigned spin = argc > 2 ? atoi(argv[2]) : 200000;
	const unsigned blocks = 96, tpb = 256;
	unsigned *out;
	if (cudaMalloc(&out, sizeof(unsigned) * blocks * tpb) != cudaSuccess)
		return 2;
	for (int i = 0; i < launches; i++)
		pt_kslow<<<blocks, tpb>>>(out, spin);
	printf("{\"pt_kslow\": {\"launches\": %d, \"threads_per_launch\": %u, \"thread_execs\": %llu}}\n",
	       launches, blocks * tpb, (unsigned long long)launches * blocks * tpb);
	fflush(stdout);
	return 0; // no synchronization: the last launches still run
}

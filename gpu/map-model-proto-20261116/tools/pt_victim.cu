// pt_victim: kernels with several exit sites for the map-model prototype's
// correctness checks. Prints the expected thread executions per kernel.
//   pt_kdiv:  lanes leave through predicated exits (inline PTX "@p exit;"),
//             data-dependent spins, a partial last block
//   pt_kflat: one exit, for graph replays
// usage: pt_victim LAUNCHES GRAPH_REPLAYS
#include <cstdio>
#include <cstdlib>
#include <cuda_runtime.h>

#define CK(x)                                                                  \
	do {                                                                   \
		cudaError_t e_ = (x);                                          \
		if (e_ != cudaSuccess) {                                       \
			fprintf(stderr, "%s:%d %s\n", __FILE__, __LINE__,     \
				cudaGetErrorString(e_));                       \
			exit(2);                                               \
		}                                                              \
	} while (0)

__device__ __forceinline__ void exit_if(bool c)
{
	asm volatile("{\n\t.reg .pred p;\n\tsetp.ne.u32 p, %0, 0;\n\t@p exit;\n\t}"
		     ::"r"((unsigned)c));
}

extern "C" __global__ void pt_kdiv(unsigned *out, unsigned n, unsigned salt)
{
	unsigned tid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned lane = threadIdx.x & 31;
	if (tid >= n)
		return;
	unsigned v = tid * 2654435761u ^ salt;
	// lanes below a per-warp cut leave first
	exit_if(lane < ((v >> 7) & 15));
	// a lane-dependent spin
	for (unsigned i = 0; i < (lane & 7) * 16; i++)
		v = v * 1664525u + 1013904223u;
	exit_if((v & 3) == 0);
	if ((v >> 3) & 1) {
		out[tid] = v;
		return;
	}
	for (unsigned i = 0; i < (v & 63); i++)
		v ^= v << 13, v ^= v >> 17, v ^= v << 5;
	out[tid] = v;
}

extern "C" __global__ void pt_kflat(unsigned *out, unsigned n)
{
	unsigned tid = blockIdx.x * blockDim.x + threadIdx.x;
	if (tid < n)
		out[tid] += 1;
}

int main(int argc, char **argv)
{
	int launches = argc > 1 ? atoi(argv[1]) : 10;
	int replays = argc > 2 ? atoi(argv[2]) : 100;
	const unsigned n_div = 100000;	// 391 blocks of 256, last partial
	const unsigned tpb_div = 256;
	const unsigned n_flat = 48 * 300; // blocks of 48: partial warps
	const unsigned tpb_flat = 48;
	unsigned *out;
	CK(cudaMalloc(&out, sizeof(unsigned) * n_div));
	cudaStream_t st;
	CK(cudaStreamCreateWithFlags(&st, cudaStreamNonBlocking));
	unsigned blocks_div = (n_div + tpb_div - 1) / tpb_div;
	for (int i = 0; i < launches; i++)
		pt_kdiv<<<blocks_div, tpb_div, 0, st>>>(out, n_div,
						       0x9e3779b9u * i);
	CK(cudaStreamSynchronize(st));
	// graph: two launches of pt_kflat per replay
	cudaGraph_t g;
	cudaGraphExec_t ge;
	CK(cudaStreamBeginCapture(st, cudaStreamCaptureModeGlobal));
	pt_kflat<<<n_flat / tpb_flat, tpb_flat, 0, st>>>(out, n_flat);
	pt_kflat<<<n_flat / tpb_flat, tpb_flat, 0, st>>>(out, n_flat);
	CK(cudaStreamEndCapture(st, &g));
	CK(cudaGraphInstantiate(&ge, g, 0));
	for (int i = 0; i < replays; i++)
		CK(cudaGraphLaunch(ge, st));
	CK(cudaStreamSynchronize(st));
	CK(cudaGetLastError());
	// capture stress (argv[3] loops): global-mode captures back to back,
	// while the agent's delivery thread launches and synchronizes; any
	// capture another thread invalidated is counted
	int loops = argc > 3 ? atoi(argv[3]) : 0, invalidated = 0, other = 0;
	for (int i = 0; i < loops; i++) {
		cudaGraph_t g2 = nullptr;
		cudaGraphExec_t ge2 = nullptr;
		CK(cudaStreamBeginCapture(st, cudaStreamCaptureModeGlobal));
		pt_kflat<<<n_flat / tpb_flat, tpb_flat, 0, st>>>(out, n_flat);
		pt_kflat<<<n_flat / tpb_flat, tpb_flat, 0, st>>>(out, n_flat);
		cudaError_t e = cudaStreamEndCapture(st, &g2);
		if (e == cudaErrorStreamCaptureInvalidated) {
			invalidated++;
			(void)cudaGetLastError();
			if (g2)
				cudaGraphDestroy(g2);
			continue;
		}
		if (e != cudaSuccess) {
			other++;
			(void)cudaGetLastError();
			continue;
		}
		CK(cudaGraphInstantiate(&ge2, g2, 0));
		CK(cudaGraphLaunch(ge2, st));
		CK(cudaStreamSynchronize(st));
		cudaGraphExecDestroy(ge2);
		cudaGraphDestroy(g2);
	}
	if (loops)
		fprintf(stderr, "capture stress: %d loops, %d invalidated, %d other errors\n",
			loops, invalidated, other);
	// every thread of the grid starts (blocks_div * tpb_div), those past n
	// leave at once; all of them run the entry and an exit
	printf("{\"pt_kdiv\": {\"launches\": %d, \"threads_per_launch\": %u, \"thread_execs\": %llu}, "
	       "\"pt_kflat\": {\"launches\": %d, \"threads_per_launch\": %u, \"thread_execs\": %llu}}\n",
	       launches, blocks_div * tpb_div,
	       (unsigned long long)launches * blocks_div * tpb_div,
	       2 * (replays + loops - invalidated - other), n_flat,
	       (unsigned long long)2 * (replays + loops - invalidated - other) *
		       n_flat);
	return 0;
}

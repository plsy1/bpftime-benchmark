// Copied unchanged (except this line) from step0/e1-ids/hog.cu.
// hog.cu: the second GPU process for case e. It keeps its own context busy with full-occupancy
// kernels that spin on %globaltimer, so the GPU must time-slice between the two contexts.
//   hog <total_s> <kernel_ms> <readyfile> <outdir>
// Every warp counts its own gaps (> gap_us between consecutive %globaltimer reads); warp 0 of
// each kernel also logs its gap intervals, which tells when this context was NOT running.
// Writes <outdir>/hog_kernels.csv and <outdir>/hog_gaps.csv.
#include <cuda_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <string>
#include <vector>

#define CK(x)                                                                                      \
	do {                                                                                       \
		cudaError_t e_ = (x);                                                              \
		if (e_ != cudaSuccess) {                                                           \
			fprintf(stderr, "hog: CUDA error %s at line %d: %s\n", cudaGetErrorString(e_), \
				__LINE__, #x);                                                     \
			exit(2);                                                                   \
		}                                                                                  \
	} while (0)

__device__ __forceinline__ unsigned long long gt()
{
	unsigned long long t;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	return t;
}

struct KSum {
	unsigned long long t0, t1, max_gap_w0, iters_w0;
	unsigned int gaps_w0, gaps_all, ngap_log, gap_overflow, capped;
	unsigned int pad;
};
#define GAPCAP 2048

__device__ __noinline__ void hog_warp(KSum *ks, unsigned long long *gaplog, unsigned long long dur_ns,
				      unsigned long long gap_ns, unsigned long long cap_iters, volatile int *ready);

__global__ void k_hog(KSum *ks, unsigned long long *gaplog, unsigned long long dur_ns, unsigned long long gap_ns,
		      unsigned long long cap_iters, volatile int *ready)
{
	if ((threadIdx.x & 31) == 0)
		hog_warp(ks, gaplog, dur_ns, gap_ns, cap_iters, ready);
	__syncwarp();
}

__device__ __noinline__ void hog_warp(KSum *ks, unsigned long long *gaplog, unsigned long long dur_ns,
				      unsigned long long gap_ns, unsigned long long cap_iters, volatile int *ready)
{
	bool w0 = (blockIdx.x == 0 && threadIdx.x == 0);
	unsigned long long t0 = gt(), prev = t0, it = 0, maxg = 0;
	unsigned int gaps = 0, nlog = 0, over = 0;
	if (w0 && ready) {
		*ready = 1;
		__threadfence_system();
	}
	for (; it < cap_iters; ++it) {
		unsigned long long now = gt();
		if (now > prev && now - prev > gap_ns) {
			gaps++;
			if (now - prev > maxg)
				maxg = now - prev;
			if (w0) {
				if (nlog < GAPCAP) {
					gaplog[2 * nlog] = prev;
					gaplog[2 * nlog + 1] = now;
					nlog++;
				} else {
					over++;
				}
			}
		}
		prev = now;
		if (now - t0 >= dur_ns)
			break;
		__nanosleep(1000); // as in ids case e: no warp starves another
	}
	atomicAdd(&ks->gaps_all, gaps);
	if (it >= cap_iters)
		atomicAdd(&ks->capped, 1u);
	if (w0) {
		ks->t0 = t0;
		ks->t1 = prev;
		ks->max_gap_w0 = maxg;
		ks->iters_w0 = it;
		ks->gaps_w0 = gaps;
		ks->ngap_log = nlog;
		ks->gap_overflow = over;
	}
}

static double now_s()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv)
{
	if (argc < 5) {
		fprintf(stderr, "usage: %s <total_s> <kernel_ms> <readyfile> <outdir> [gap_us=50]\n", argv[0]);
		return 1;
	}
	double total_s = atof(argv[1]);
	double kernel_ms = atof(argv[2]);
	std::string ready_path = argv[3], out = argv[4];
	double gap_us = argc > 5 ? atof(argv[5]) : 50.0;
	int nk = (int)(total_s * 1e3 / kernel_ms + 0.5);
	if (nk < 1)
		nk = 1;
	cudaDeviceProp prop;
	CK(cudaGetDeviceProperties(&prop, 0));
	const int bs = 128;
	int occ = 0;
	CK(cudaOccupancyMaxActiveBlocksPerMultiprocessor(&occ, k_hog, bs, 0));
	int blocks = occ * prop.multiProcessorCount;
	KSum *d_ks;
	unsigned long long *d_gap;
	CK(cudaMalloc(&d_ks, sizeof(KSum) * nk));
	CK(cudaMemset(d_ks, 0, sizeof(KSum) * nk));
	CK(cudaDeviceSynchronize()); // legacy-stream memset vs the non-blocking stream s below
	CK(cudaMalloc(&d_gap, sizeof(unsigned long long) * 2 * GAPCAP * nk));
	int *h_ready;
	CK(cudaHostAlloc(&h_ready, sizeof(int), cudaHostAllocMapped));
	*(volatile int *)h_ready = 0;
	int *d_ready;
	CK(cudaHostGetDevicePointer(&d_ready, h_ready, 0));
	unsigned long long dur_ns = (unsigned long long)(kernel_ms * 1e6);
	// a cap far above any plausible iteration count for kernel_ms (globaltimer stuck guard)
	unsigned long long cap_iters = 1ull << 22;
	cudaStream_t s;
	CK(cudaStreamCreateWithFlags(&s, cudaStreamNonBlocking));
	double t0 = now_s();
	// all kernels queued at once so the context never idles between them
	for (int k = 0; k < nk; k++) {
		k_hog<<<blocks, bs, 0, s>>>(d_ks + k, d_gap + (size_t)2 * GAPCAP * k, dur_ns,
					    (unsigned long long)(gap_us * 1e3), cap_iters, k == 0 ? d_ready : nullptr);
		CK(cudaGetLastError());
	}
	// ready = the first kernel is running on the GPU (host poll capped at 10 s)
	bool ok = false;
	for (int i = 0; i < 10000; i++) {
		if (*(volatile int *)h_ready) {
			ok = true;
			break;
		}
		usleep(1000);
	}
	if (ok) {
		FILE *f = fopen(ready_path.c_str(), "w");
		if (f) {
			fprintf(f, "%d\n", (int)getpid());
			fclose(f);
		}
	}
	printf("hog: pid %d, %d kernels x %.1f ms, %d blocks x %d, first kernel running: %s\n", (int)getpid(), nk,
	       kernel_ms, blocks, bs, ok ? "yes" : "NO (10 s)");
	fflush(stdout);
	CK(cudaStreamSynchronize(s));
	double t1 = now_s();
	std::vector<KSum> ks(nk);
	std::vector<unsigned long long> gl((size_t)2 * GAPCAP * nk);
	CK(cudaMemcpy(ks.data(), d_ks, sizeof(KSum) * nk, cudaMemcpyDeviceToHost));
	CK(cudaMemcpy(gl.data(), d_gap, sizeof(unsigned long long) * gl.size(), cudaMemcpyDeviceToHost));
	FILE *fk = fopen((out + "/hog_kernels.csv").c_str(), "w");
	FILE *fg = fopen((out + "/hog_gaps.csv").c_str(), "w");
	if (!fk || !fg) {
		perror("hog output");
		return 2;
	}
	fputs("kernel,t0,t1,iters_w0,gaps_w0,gaps_all,max_gap_w0,ngap_log,gap_overflow,capped_warps,blocks,bs\n", fk);
	fputs("kernel,t_prev,t_now\n", fg);
	unsigned long long tg = 0;
	for (int k = 0; k < nk; k++) {
		const KSum &x = ks[k];
		fprintf(fk, "%d,%llu,%llu,%llu,%u,%u,%llu,%u,%u,%u,%d,%d\n", k, x.t0, x.t1, x.iters_w0, x.gaps_w0,
			x.gaps_all, x.max_gap_w0, x.ngap_log, x.gap_overflow, x.capped, blocks, bs);
		tg += x.gaps_w0;
		for (unsigned i = 0; i < x.ngap_log && i < GAPCAP; i++)
			fprintf(fg, "%d,%llu,%llu\n", k, gl[(size_t)2 * GAPCAP * k + 2 * i],
				gl[(size_t)2 * GAPCAP * k + 2 * i + 1]);
	}
	fclose(fk);
	fclose(fg);
	printf("hog: done in %.2f s, warp-0 gaps %llu\n", t1 - t0, tg);
	CK(cudaFreeHost(h_ready));
	return 0;
}

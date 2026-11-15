// Copied from step0/e3-copyout/partB/captest.cu (sha256 below in ../README section); unchanged otherwise.
// captest.cu: step 0, check 3, part B0. A small capture-and-replay loop in
// the shape llama.cpp uses (cudaStreamBeginCapture on a stream of its own,
// K kernel launches, cudaStreamEndCapture, cudaGraphInstantiate the first
// time and cudaGraphExecUpdate after, cudaGraphLaunch), run under the
// step0_copier shim. Unlike llama.cpp it never aborts on an error: it counts
// the result of every call, so a copier that invalidates captures shows up
// as counted errors instead of a crash.
//
// Must be linked with the shared cudart (-cudart shared) so that the shim's
// interposers see the same calls as in llama.cpp.
//
//   captest --mode relaxed|global|thread_local --stream nonblocking|blocking
//           --seconds S --kernels K --out FILE
#include <cuda_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_s()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// a few microseconds of work, bounded
__global__ void tiny(float *buf, int k)
{
	float x = buf[threadIdx.x] + (float)k;
	for (int i = 0; i < 200; ++i)
		x = x * 0.999f + 0.001f;
	buf[threadIdx.x] = x;
}

struct C {
	unsigned long calls = 0, errors = 0;
	int first = 0;
	void add(cudaError_t e)
	{
		calls++;
		if (e != cudaSuccess) {
			if (!errors)
				first = (int)e;
			errors++;
		}
	}
};

int main(int argc, char **argv)
{
	const char *mode_s = "relaxed", *stream_s = "nonblocking",
		   *out = "/dev/stdout";
	double seconds = 3;
	int K = 200;
	for (int i = 1; i + 1 < argc; i += 2) {
		if (!strcmp(argv[i], "--mode"))
			mode_s = argv[i + 1];
		else if (!strcmp(argv[i], "--stream"))
			stream_s = argv[i + 1];
		else if (!strcmp(argv[i], "--seconds"))
			seconds = atof(argv[i + 1]);
		else if (!strcmp(argv[i], "--kernels"))
			K = atoi(argv[i + 1]);
		else if (!strcmp(argv[i], "--out"))
			out = argv[i + 1];
		else {
			fprintf(stderr, "bad option %s\n", argv[i]);
			return 1;
		}
	}
	cudaStreamCaptureMode mode = cudaStreamCaptureModeRelaxed;
	if (!strcmp(mode_s, "global"))
		mode = cudaStreamCaptureModeGlobal;
	else if (!strcmp(mode_s, "thread_local"))
		mode = cudaStreamCaptureModeThreadLocal;
	cudaStream_t s;
	if (!strcmp(stream_s, "blocking"))
		cudaStreamCreate(&s);
	else
		cudaStreamCreateWithFlags(&s, cudaStreamNonBlocking);
	float *buf;
	if (cudaMalloc(&buf, 32 * sizeof(float)) != cudaSuccess) {
		fprintf(stderr, "cudaMalloc failed\n");
		return 2;
	}
	cudaMemset(buf, 0, 32 * sizeof(float));
	cudaDeviceSynchronize();
	// let the shim's copier finish its set-up first (it starts once the
	// primary context is active); 0.5 s is ample, its log shows when
	{
		double t = now_s();
		while (now_s() - t < 0.5)
			;
	}
	C begin, launchk, end, inst, upd, glaunch, sync;
	unsigned long iters = 0, invalidated = 0, status_errors = 0;
	cudaGraphExec_t exec = nullptr;
	const double t0 = now_s();
	double cap_s = 0;
	while (now_s() - t0 < seconds && iters < 1000000) {
		iters++;
		const double c0 = now_s();
		cudaError_t e = cudaStreamBeginCapture(s, mode);
		begin.add(e);
		if (e != cudaSuccess) {
			cudaGetLastError();
			continue;
		}
		for (int k = 0; k < K; ++k) {
			tiny<<<1, 32, 0, s>>>(buf, k);
			launchk.add(cudaGetLastError());
		}
		cudaStreamCaptureStatus cs;
		if (cudaStreamIsCapturing(s, &cs) != cudaSuccess)
			status_errors++;
		else if (cs == cudaStreamCaptureStatusInvalidated)
			invalidated++;
		cudaGraph_t g = nullptr;
		e = cudaStreamEndCapture(s, &g);
		end.add(e);
		cap_s += now_s() - c0;
		if (e != cudaSuccess || !g) {
			cudaGetLastError();
			if (g)
				cudaGraphDestroy(g);
			continue;
		}
		if (!exec) {
			e = cudaGraphInstantiate(&exec, g, 0);
			inst.add(e);
		} else {
			cudaGraphExecUpdateResultInfo info;
			e = cudaGraphExecUpdate(exec, g, &info);
			upd.add(e);
			if (e != cudaSuccess) {
				cudaGetLastError();
				cudaGraphExecDestroy(exec);
				exec = nullptr;
				e = cudaGraphInstantiate(&exec, g, 0);
				inst.add(e);
			}
		}
		if (e == cudaSuccess && exec) {
			glaunch.add(cudaGraphLaunch(exec, s));
			sync.add(cudaStreamSynchronize(s));
		} else {
			cudaGetLastError();
		}
		cudaGraphDestroy(g);
	}
	const double el = now_s() - t0;
	FILE *f = fopen(out, "a");
	if (!f)
		return 1;
#define J(n, c)                                                                \
	fprintf(f, "\"%s\":{\"calls\":%lu,\"errors\":%lu,\"first_err\":%d},", n, \
		c.calls, c.errors, c.first)
	fprintf(f, "{\"test\":\"captest\",\"mode\":\"%s\",\"stream\":\"%s\","
		   "\"kernels\":%d,\"seconds\":%.3f,\"iters\":%lu,"
		   "\"capture_open_share\":%.3f,\"invalidated_seen\":%lu,"
		   "\"status_errors\":%lu,",
		mode_s, stream_s, K, el, iters, cap_s / el, invalidated,
		status_errors);
	J("begin", begin);
	J("kernel_launch", launchk);
	J("end", end);
	J("instantiate", inst);
	J("exec_update", upd);
	J("graph_launch", glaunch);
	J("sync", sync);
	fprintf(f, "\"done\":1}\n");
	fclose(f);
	return 0;
}

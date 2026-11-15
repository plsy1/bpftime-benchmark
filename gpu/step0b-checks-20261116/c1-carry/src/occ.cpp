// occ: part (d). For every cubin given, loads it (cuModuleLoad; no kernel
// launch) and reports, for one kernel, CU_FUNC_ATTRIBUTE_NUM_REGS,
// LOCAL_SIZE_BYTES and cuOccupancyMaxActiveBlocksPerMultiprocessor at the
// block sizes given; also the context's CU_LIMIT_STACK_SIZE before any load
// and after each load.
//   occ KERNEL BLOCKSIZES(comma) CUBIN...      -> one CSV line per cubin
#include <cuda.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#define CK(call)                                                                  \
	do {                                                                      \
		CUresult rc_ = (call);                                            \
		if (rc_ != CUDA_SUCCESS) {                                        \
			const char *s_ = nullptr;                                 \
			cuGetErrorString(rc_, &s_);                               \
			fprintf(stderr, "%s:%d: %s -> %d (%s)\n", __FILE__, __LINE__, \
				#call, (int)rc_, s_ ? s_ : "?");                  \
			exit(10);                                                 \
		}                                                                 \
	} while (0)

int main(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "usage: %s KERNEL BLOCKSIZES CUBIN...\n", argv[0]);
		return 2;
	}
	const std::string kname = argv[1];
	std::vector<int> bs;
	for (std::string s = argv[2]; !s.empty();) {
		size_t c = s.find(',');
		bs.push_back(atoi(s.substr(0, c).c_str()));
		s = c == std::string::npos ? "" : s.substr(c + 1);
	}
	CK(cuInit(0));
	CUdevice dev;
	CK(cuDeviceGet(&dev, 0));
	CUcontext ctx;
	CK(cuDevicePrimaryCtxRetain(&ctx, dev));
	CK(cuCtxSetCurrent(ctx));
	size_t stack0 = 0;
	CK(cuCtxGetLimit(&stack0, CU_LIMIT_STACK_SIZE));
	int nsm = 0, thr = 0;
	CK(cuDeviceGetAttribute(&nsm, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, dev));
	CK(cuDeviceGetAttribute(&thr, CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR, dev));
	printf("cubin,regs,local_bytes,static_smem,stack_limit_before,stack_limit_after,sm_count,"
	       "max_threads_per_sm");
	for (int b : bs)
		printf(",blocks_per_sm_at_%d", b);
	printf("\n");
	for (int i = 3; i < argc; i++) {
		CUmodule m;
		CK(cuModuleLoad(&m, argv[i]));
		CUfunction f;
		CK(cuModuleGetFunction(&f, m, kname.c_str()));
		int regs = 0, local = 0, smem = 0;
		CK(cuFuncGetAttribute(&regs, CU_FUNC_ATTRIBUTE_NUM_REGS, f));
		CK(cuFuncGetAttribute(&local, CU_FUNC_ATTRIBUTE_LOCAL_SIZE_BYTES, f));
		CK(cuFuncGetAttribute(&smem, CU_FUNC_ATTRIBUTE_SHARED_SIZE_BYTES, f));
		size_t stack1 = 0;
		CK(cuCtxGetLimit(&stack1, CU_LIMIT_STACK_SIZE));
		printf("%s,%d,%d,%d,%zu,%zu,%d,%d", argv[i], regs, local, smem, stack0, stack1, nsm, thr);
		for (int b : bs) {
			int occ = 0;
			CK(cuOccupancyMaxActiveBlocksPerMultiprocessor(&occ, f, b, 0));
			printf(",%d", occ);
		}
		printf("\n");
		CK(cuModuleUnload(m));
	}
	return 0;
}

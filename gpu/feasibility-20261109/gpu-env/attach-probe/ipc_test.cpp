// What bpftime's IPC-based GPU maps (BPF_MAP_TYPE_GPU_ARRAY_MAP 1503,
// PERGPUTD_ARRAY_MAP 1502, GPU_HASH_MAP 1501) do across processes:
// syscall server: cuMemAlloc + cuIpcGetMemHandle  (nv_gpu_shared_array_map.cpp:42-63)
// agent:          cuIpcOpenMemHandle               (nv_gpu_shared_array_map.cpp:293)
// usage: ipc_test            (parent)
//        ipc_test child HEX  (spawned by parent)
#include <cuda.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;

static const char *name(CUresult r)
{
	const char *n = nullptr;
	cuGetErrorName(r, &n);
	return n ? n : "?";
}

int main(int argc, char **argv)
{
	setvbuf(stdout, nullptr, _IONBF, 0);
	CUresult r = cuInit(0);
	CUdevice dev;
	CUcontext ctx;
	cuDeviceGet(&dev, 0);
	cuDevicePrimaryCtxRetain(&ctx, dev);
	cuCtxSetCurrent(ctx);
	if (argc == 3 && strcmp(argv[1], "child") == 0) {
		CUipcMemHandle h;
		for (int i = 0; i < (int)sizeof(h.reserved); i++) {
			unsigned v;
			sscanf(argv[2] + 2 * i, "%2x", &v);
			h.reserved[i] = (char)v;
		}
		CUdeviceptr p = 0;
		r = cuIpcOpenMemHandle(&p, h,
				       CU_IPC_MEM_LAZY_ENABLE_PEER_ACCESS);
		printf("[child %d] cuIpcOpenMemHandle -> %d %s ptr=%#llx\n",
		       getpid(), (int)r, name(r), (unsigned long long)p);
		if (r == CUDA_SUCCESS) {
			unsigned v = 0;
			r = cuMemcpyDtoH(&v, p, sizeof(v));
			printf("[child] read -> %d %s value=%#x (expect 0x1234abcd)\n",
			       (int)r, name(r), v);
			cuIpcCloseMemHandle(p);
		}
		return 0;
	}
	printf("cuInit -> %d %s\n", (int)r, name(r));
	CUdeviceptr d;
	r = cuMemAlloc(&d, 1 << 20);
	printf("cuMemAlloc -> %d %s\n", (int)r, name(r));
	cuMemsetD32(d, 0x1234abcd, 1);
	cuCtxSynchronize();
	CUipcMemHandle h;
	memset(&h, 0, sizeof(h));
	r = cuIpcGetMemHandle(&h, d);
	printf("cuIpcGetMemHandle -> %d %s\n", (int)r, name(r));
	CUipcEventHandle eh;
	CUevent ev;
	cuEventCreate(&ev, CU_EVENT_INTERPROCESS | CU_EVENT_DISABLE_TIMING);
	CUresult r2 = cuIpcGetEventHandle(&eh, ev);
	printf("cuIpcGetEventHandle -> %d %s\n", (int)r2, name(r2));
	if (r != CUDA_SUCCESS)
		return 0;
	std::string hex;
	char b[3];
	for (int i = 0; i < (int)sizeof(h.reserved); i++) {
		snprintf(b, sizeof(b), "%02x", (unsigned char)h.reserved[i]);
		hex += b;
	}
	char *cargv[] = { argv[0], (char *)"child", hex.data(), nullptr };
	pid_t pid;
	if (posix_spawn(&pid, argv[0], nullptr, nullptr, cargv, environ) == 0) {
		int st = 0;
		waitpid(pid, &st, 0);
		printf("child exit=%d\n", WIFEXITED(st) ? WEXITSTATUS(st) : -1);
	}
	return 0;
}

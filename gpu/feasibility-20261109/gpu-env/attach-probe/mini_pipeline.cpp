// Replays the device half of bpftime's CUDA attach pipeline on this board,
// with bpftime's own calls and arguments, minus Frida and the eBPF front end:
//   fatbin_record::compile_ptxs  -> nvPTXCompilerCompile(--gpu-name=<sm> --verbose -O3)
//   fatbin_record::try_loading_ptxs -> cuModuleLoadDataEx + cuModuleGetGlobal
//        ("constData", "map_info") + cuMemcpyHtoD
//   bpftime_shm::register_cuda_host_memory -> cudaSetDeviceFlags(MapHost),
//        prefault, cudaHostRegister(whole segment, cudaHostRegisterMapped)
//   CUDAContext ctor -> cudaHostGetDevicePointer(CommSharedMem)
//   nv_gpu_shared_array_host_map_impl -> cuMemHostGetDevicePointer(interior)
//   create_map_basic_info(1024) -> MapBasicInfo[1024] (40 B each)
//   mirror_cuda_memcpy_to_symbol -> cuModuleGetGlobal(patched, "d_N") + HtoD
//   cuda_launch_kernel_common -> cuLaunchKernel(patched CUfunction, ...)
// usage: mini_pipeline PATCHED_PTX SM KERNEL
#include <cuda.h>
#include <cuda_runtime.h>
#include <nvPTXCompiler.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

#define CK(x)                                                                  \
	do {                                                                   \
		CUresult r_ = (x);                                             \
		const char *n_ = nullptr;                                      \
		cuGetErrorName(r_, &n_);                                       \
		printf("  %-58s -> %d %s\n", #x, (int)r_, n_ ? n_ : "?");      \
		if (r_ != CUDA_SUCCESS)                                        \
			return 2;                                              \
	} while (0)
#define CKR(x)                                                                 \
	do {                                                                   \
		cudaError_t r_ = (x);                                          \
		printf("  %-58s -> %d %s\n", #x, (int)r_,                      \
		       cudaGetErrorString(r_));                                \
		if (r_ != cudaSuccess && r_ != cudaErrorSetOnActiveProcess)    \
			return 2;                                              \
	} while (0)

struct MapBasicInfo { // attach/nv_attach_impl/nv_attach_impl.hpp:45
	bool enabled;
	int key_size;
	int value_size;
	int max_entries;
	int map_type;
	void *extra_buffer;
	uint64_t max_thread_count;
};
static_assert(sizeof(MapBasicInfo) == 40, "layout");


static const char *g_seg_tag = nullptr;
static char *g_seg = nullptr;
static void seg_check(const char *where)
{
	char want[64];
	snprintf(want, sizeof(want), "%lx-", (unsigned long)g_seg);
	FILE *f = fopen("/proc/self/maps", "r");
	char line[512];
	const char *state = "MISSING";
	static char found[512];
	while (f && fgets(line, sizeof(line), f)) {
		if (strncmp(line, want, strlen(want)) == 0) {
			state = "present";
			snprintf(found, sizeof(found), "%s", line);
			break;
		}
	}
	if (f)
		fclose(f);
	printf("  [seg %p after %-28s] %s %s", (void *)g_seg, where, state,
	       strcmp(state, "present") == 0 ? found : "\n");
}

int main(int argc, char **argv)
{
	setvbuf(stdout, nullptr, _IONBF, 0);
	if (argc < 4) {
		fprintf(stderr, "usage: %s PTX SM KERNEL\n", argv[0]);
		return 1;
	}
	std::ifstream ifs(argv[1]);
	std::stringstream ss;
	ss << ifs.rdbuf();
	std::string ptx = ss.str();
	std::string sm = argv[2];
	const char *kernel_name = argv[3];

	// --- segment: like boost managed_shared_memory in /dev/shm, MAP_SHARED
	const size_t seg_size = 16u << 20;
	char shm_name[64];
	snprintf(shm_name, sizeof(shm_name), "/gpu-env-attach-probe-%d", getpid());
	int fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0600);
	if (fd < 0 || ftruncate(fd, seg_size) != 0) {
		perror("shm");
		return 1;
	}
	auto *seg = (char *)mmap(nullptr, seg_size, PROT_READ | PROT_WRITE,
				 MAP_SHARED, fd, 0);
	close(fd);
	shm_unlink(shm_name);
	if (seg == MAP_FAILED) {
		perror("mmap");
		return 1;
	}
	volatile unsigned char sink = 0;
	for (size_t i = 0; i < seg_size; i += 4096)
		sink ^= ((volatile unsigned char *)seg)[i];
	const size_t comm_off = 0x1000, map_off = 0x40010; // interior offsets
	memset(seg, 0, seg_size);
	g_seg = seg;
	seg_check("mmap+prefault+memset");

	CKR(cudaSetDeviceFlags(cudaDeviceMapHost));
	seg_check("cudaSetDeviceFlags");
	CKR(cudaFree(0));
	seg_check("cudaFree(0) ctx init");
	CKR(cudaHostRegister(seg, seg_size, cudaHostRegisterMapped));
	seg_check("cudaHostRegister");
	void *comm_dev = nullptr;
	CKR(cudaHostGetDevicePointer(&comm_dev, seg + comm_off, 0));
	CUdeviceptr map_dev = 0;
	CK(cuMemHostGetDevicePointer(&map_dev, seg + map_off, 0));
	printf("  comm host=%p dev=%p | map host=%p dev=%#llx\n",
	       (void *)(seg + comm_off), comm_dev, (void *)(seg + map_off),
	       (unsigned long long)map_dev);

	// --- compile_ptxs
	unsigned maj = 0, min = 0;
	nvPTXCompilerGetVersion(&maj, &min);
	nvPTXCompilerHandle comp = nullptr;
	if (nvPTXCompilerCreate(&comp, ptx.size(), ptx.c_str()) !=
	    NVPTXCOMPILE_SUCCESS) {
		printf("  nvPTXCompilerCreate failed\n");
		return 3;
	}
	std::string gpu = "--gpu-name=" + sm;
	const char *opts[] = { gpu.c_str(), "--verbose", "-O3" };
	auto st = nvPTXCompilerCompile(comp, 3, opts);
	printf("  nvPTXCompiler %u.%u compile(%s --verbose -O3) -> %d\n", maj,
	       min, gpu.c_str(), (int)st);
	if (st != NVPTXCOMPILE_SUCCESS) {
		size_t n = 0;
		nvPTXCompilerGetErrorLogSize(comp, &n);
		std::string log(n + 1, '\0');
		nvPTXCompilerGetErrorLog(comp, log.data());
		printf("  error log: %s\n", log.c_str());
		return 3;
	}
	size_t elf_size = 0;
	nvPTXCompilerGetCompiledProgramSize(comp, &elf_size);
	std::vector<char> elf(elf_size);
	nvPTXCompilerGetCompiledProgram(comp, elf.data());
	nvPTXCompilerDestroy(&comp);
	printf("  compiled ELF %zu bytes\n", elf_size);
	seg_check("nvPTXCompiler");

	// --- try_loading_ptxs
	CUmodule mod;
	char info_buf[8192] = { 0 }, err_buf[8192] = { 0 };
	CUjit_option o[] = { CU_JIT_INFO_LOG_BUFFER,
			     CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES,
			     CU_JIT_ERROR_LOG_BUFFER,
			     CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES };
	void *ov[] = { info_buf, (void *)sizeof(info_buf), err_buf,
		       (void *)sizeof(err_buf) };
	CK(cuModuleLoadDataEx(&mod, elf.data(), 4, o, ov));
	seg_check("cuModuleLoadDataEx");
	CUdeviceptr const_data, map_info;
	size_t const_data_size = 0, map_info_size = 0;
	CK(cuModuleGetGlobal(&const_data, &const_data_size, mod, "constData"));
	CK(cuModuleGetGlobal(&map_info, &map_info_size, mod, "map_info"));
	printf("  constData size=%zu map_info size=%zu\n", const_data_size,
	       map_info_size);
	uintptr_t shared_mem_ptr = (uintptr_t)comm_dev;
	CK(cuMemcpyHtoD(const_data, &shared_mem_ptr, const_data_size));
	std::vector<MapBasicInfo> infos(1024);
	memset(infos.data(), 0, infos.size() * sizeof(MapBasicInfo));
	infos[3] = MapBasicInfo{ true, 4, 8, 16, 1513 /* GPU_ARRAY_HOST_MAP */,
				 (void *)map_dev, 0 };
	CK(cuMemcpyHtoD(map_info, infos.data(), map_info_size));
	seg_check("constData/map_info HtoD");

	// --- the app's own globals live separately in the patched module
	CUdeviceptr d_n, d_counter;
	size_t sz;
	CK(cuModuleGetGlobal(&d_n, &sz, mod, "d_N"));
	CK(cuModuleGetGlobal(&d_counter, &sz, mod, "d_counter"));
	int n = 1024, zero = 0;
	CK(cuMemcpyHtoD(d_n, &n, sizeof(n)));
	CK(cuMemcpyHtoD(d_counter, &zero, sizeof(zero)));

	CUfunction f;
	CK(cuModuleGetFunction(&f, mod, kernel_name));
	int local = -1, regs = -1;
	cuFuncGetAttribute(&local, CU_FUNC_ATTRIBUTE_LOCAL_SIZE_BYTES, f);
	cuFuncGetAttribute(&regs, CU_FUNC_ATTRIBUTE_NUM_REGS, f);
	printf("  patched kernel: local=%d B/thread regs=%d\n", local, regs);

	CUdeviceptr a, b, c;
	CK(cuMemAlloc(&a, n * sizeof(float)));
	CK(cuMemAlloc(&b, n * sizeof(float)));
	CK(cuMemAlloc(&c, n * sizeof(float)));
	void *args[] = { &a, &b, &c };
	seg_check("cuMemAlloc x3");
	size_t free0 = 0, tot0 = 0, lim0 = 0;
	cuMemGetInfo(&free0, &tot0);
	cuCtxGetLimit(&lim0, CU_LIMIT_STACK_SIZE);
	CK(cuLaunchKernel(f, 4, 1, 1, 256, 1, 1, 0, nullptr, args, nullptr));
	seg_check("cuLaunchKernel");
	CK(cuCtxSynchronize());
	seg_check("cuCtxSynchronize");
	size_t free1 = 0, tot1 = 0, lim1 = 0;
	cuMemGetInfo(&free1, &tot1);
	cuCtxGetLimit(&lim1, CU_LIMIT_STACK_SIZE);
	printf("  MEM free before launch=%zu MiB after=%zu MiB (delta %lld MiB); CU_LIMIT_STACK_SIZE %zu -> %zu\n", free0 >> 20, free1 >> 20, ((long long)free0 - (long long)free1) / (1 << 20), lim0, lim1);

	int counter = -1;
	CK(cuMemcpyDtoH(&counter, d_counter, sizeof(counter)));
	seg_check("cuMemcpyDtoH counter");
	auto *host_map = (volatile uint64_t *)(seg + map_off);
	printf("RESULT kernel counter=%d (expect 1024) | host map[0]=%llu "
	       "(expect 1024 probe hits) | host map[1] globaltimer=%llu\n",
	       counter, (unsigned long long)host_map[0],
	       (unsigned long long)host_map[1]);
	const uint64_t hits = host_map[0];
	CK(cuModuleUnload(mod));
	CKR(cudaHostUnregister(seg));
	munmap(seg, seg_size);
	return (counter == 1024 && hits == 1024) ? 0 : 4;
}

// ptxprobe.cpp - for each PTX file: (P2) nvPTXCompiler --gpu-name=sm_87 -> ELF -> cuModuleLoadDataEx -> run,
//                                    (P3) cuModuleLoadDataEx(PTX text) via driver JIT -> run.
// Runs kernel "k" (stores 42 through its pointer argument) when the module has it.
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cuda.h>
#include <nvPTXCompiler.h>

static const char *cuerr(CUresult r)
{
	const char *s = "?";
	cuGetErrorString(r, &s);
	return s;
}
static std::string oneline(const char *s, size_t max = 300)
{
	std::string o;
	for (const char *p = s; *p && o.size() < max; p++)
		o += (*p == '\n') ? ' ' : *p;
	return o;
}
static std::string run_k(CUmodule mod)
{
	CUfunction f;
	CUresult r = cuModuleGetFunction(&f, mod, "k");
	if (r != CUDA_SUCCESS)
		return std::string("no kernel k (") + cuerr(r) + ")";
	CUdeviceptr d;
	cuMemAlloc(&d, 4);
	cuMemsetD32(d, 0, 1);
	void *args[] = {&d};
	r = cuLaunchKernel(f, 1, 1, 1, 1, 1, 1, 0, 0, args, nullptr);
	CUresult s = cuCtxSynchronize();
	unsigned v = 0;
	cuMemcpyDtoH(&v, d, 4);
	cuMemFree(d);
	char b[160];
	snprintf(b, sizeof b, "launch=%s sync=%s value=%u%s", cuerr(r), cuerr(s), v, v == 42 ? " RAN-OK" : " WRONG");
	return b;
}

int main(int argc, char **argv)
{
	const char *gpu = getenv("GPU_NAME") ? getenv("GPU_NAME") : "sm_87";
	unsigned maj = 0, min = 0;
	nvPTXCompilerGetVersion(&maj, &min);
	CUresult r = cuInit(0);
	CUdevice dev;
	cuDeviceGet(&dev, 0);
	CUcontext ctx;
	CUctxCreateParams *cp = nullptr;
	r = cuCtxCreate(&ctx, cp, 0, dev);
	int drv = 0;
	cuDriverGetVersion(&drv);
	printf("nvPTXCompilerGetVersion=%u.%u cuDriverGetVersion=%d cuCtxCreate=%s gpu-name=%s\n", maj, min, drv, cuerr(r),
	       gpu);
	for (int i = 1; i < argc; i++) {
		std::ifstream in(argv[i]);
		std::stringstream ss;
		ss << in.rdbuf();
		std::string ptx = ss.str();
		const char *base = strrchr(argv[i], '/') ? strrchr(argv[i], '/') + 1 : argv[i];
		// ---- P2: nvPTXCompiler (same options as nv_attach_impl.cpp:1175-1177)
		std::string p2;
		{
			nvPTXCompilerHandle h = nullptr;
			nvPTXCompileResult cr = nvPTXCompilerCreate(&h, ptx.size(), ptx.c_str());
			std::string opt = std::string("--gpu-name=") + gpu;
			const char *opts[] = {opt.c_str(), "--verbose"};
			cr = nvPTXCompilerCompile(h, 2, opts);
			if (cr != NVPTXCOMPILE_SUCCESS) {
				size_t es = 0;
				nvPTXCompilerGetErrorLogSize(h, &es);
				std::string el(es + 1, 0);
				if (es)
					nvPTXCompilerGetErrorLog(h, el.data());
				p2 = "compile FAIL (" + std::to_string((int)cr) + "): " + oneline(el.c_str());
			} else {
				size_t sz = 0;
				nvPTXCompilerGetCompiledProgramSize(h, &sz);
				std::vector<char> elf(sz);
				nvPTXCompilerGetCompiledProgram(h, elf.data());
				CUmodule m;
				char eb[4096] = {0}, ib[4096] = {0};
				CUjit_option o[] = {CU_JIT_INFO_LOG_BUFFER, CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES,
						    CU_JIT_ERROR_LOG_BUFFER, CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES};
				void *ov[] = {ib, (void *)sizeof ib, eb, (void *)sizeof eb};
				CUresult lr = cuModuleLoadDataEx(&m, elf.data(), 4, o, ov);
				p2 = "compile OK elf=" + std::to_string(sz) + "B; load(ELF)=" + cuerr(lr);
				if (lr == CUDA_SUCCESS) {
					p2 += "; " + run_k(m);
					cuModuleUnload(m);
				} else {
					p2 += " err=" + oneline(eb);
				}
			}
			nvPTXCompilerDestroy(&h);
		}
		// ---- P3: driver JIT of PTX text
		std::string p3;
		{
			CUmodule m;
			char eb[4096] = {0}, ib[4096] = {0};
			CUjit_option o[] = {CU_JIT_INFO_LOG_BUFFER, CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES, CU_JIT_ERROR_LOG_BUFFER,
					    CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES};
			void *ov[] = {ib, (void *)sizeof ib, eb, (void *)sizeof eb};
			CUresult lr = cuModuleLoadDataEx(&m, ptx.c_str(), 4, o, ov);
			p3 = std::string("load(PTX)=") + std::to_string((int)lr) + " " + cuerr(lr);
			if (lr == CUDA_SUCCESS) {
				p3 += "; " + run_k(m);
				cuModuleUnload(m);
			} else {
				p3 += " err=" + oneline(eb);
			}
			// plain cuModuleLoadData too (what the task literally asks)
			CUmodule m2;
			CUresult lr2 = cuModuleLoadData(&m2, ptx.c_str());
			p3 += std::string(" | cuModuleLoadData=") + std::to_string((int)lr2);
			if (lr2 == CUDA_SUCCESS)
				cuModuleUnload(m2);
		}
		printf("%-34s P2[nvPTXCompiler]: %s\n%-34s P3[driver JIT]:   %s\n", base, p2.c_str(), "", p3.c_str());
	}
	return 0;
}

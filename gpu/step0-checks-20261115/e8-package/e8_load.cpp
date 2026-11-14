// e8_load PTX MODE N [--jit] [--info-log FILE]
// step0 check 8: can a .global appended to a patched llama.cpp module be
// compiled, loaded and found, and how much constant memory is left?
// The PTX is prepared as bpftime prepares it (rewrite_ptx_target: the first
// .target becomes sm_87; .version kept for sm_87), compiled with nvPTXCompiler
// with bpftime's options (--gpu-name=sm_87 --verbose -O3, fatbin_record.cpp),
// and the resulting cubin is loaded with cuLibraryLoadData, as the agent's
// cuLibraryLoadData hook does. --jit instead hands the PTX text to
// cuLibraryLoadData (the driver compiles it).
// MODE:
//   base      nothing appended (N ignored): the reference
//   plain     ".global .align 8 .b8 __bpftime_step0_scratch[N];" (the string as given)
//   visible   the same with .visible
//   kernel    .visible, plus a kernel __bpftime_step0_touch that writes it through the symbol
//   constpad  compile only: ".visible .const .align 8 .b8 __bpftime_step0_cpad[N];" read by a kernel
// Prints one line "E8RESULT {json}". Exit 0 unless the program itself failed.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cuda.h>
#include <nvPTXCompiler.h>

static const char *SYM = "__bpftime_step0_scratch";
static const uint64_t PATTERN = 0x5A5A5A5A5A5A5A5Aull;

static std::string rewrite_target(std::string ptx, const std::string &arch)
{
	// as attach/nv_attach_impl/nv_attach_utils.cpp rewrite_ptx_target (sm_87:
	// .version unchanged; the first .target token replaced)
	auto pos = ptx.find(".target");
	if (pos == std::string::npos)
		return ptx;
	pos += 7;
	while (pos < ptx.size() && (ptx[pos] == ' ' || ptx[pos] == '\t'))
		pos++;
	auto start = pos;
	while (pos < ptx.size() && ptx[pos] != ' ' && ptx[pos] != '\t' &&
	       ptx[pos] != '\n' && ptx[pos] != '\r' && ptx[pos] != ',')
		pos++;
	ptx.replace(start, pos - start, arch);
	return ptx;
}

static std::string touch_kernel()
{
	return R"PTX(
.visible .entry __bpftime_step0_touch(
	.param .u64 __bpftime_step0_touch_param_0,
	.param .u64 __bpftime_step0_touch_param_1
)
{
	.reg .pred 	%p<2>;
	.reg .b32 	%r<8>;
	.reg .b64 	%rd<16>;
	ld.param.u64 	%rd1, [__bpftime_step0_touch_param_0];
	ld.param.u64 	%rd2, [__bpftime_step0_touch_param_1];
	cvta.to.global.u64 	%rd1, %rd1;
	mov.u32 	%r1, %ctaid.x;
	mov.u32 	%r2, %ntid.x;
	mov.u32 	%r3, %tid.x;
	mad.lo.s32 	%r4, %r1, %r2, %r3;
	mov.u32 	%r5, %nctaid.x;
	mul.lo.s32 	%r6, %r5, %r2;
	cvt.u64.u32 	%rd3, %r4;
	cvt.u64.u32 	%rd4, %r6;
	add.s64 	%rd5, %rd2, -1;
	add.s64 	%rd6, %rd4, -1;
	mul.lo.s64 	%rd7, %rd3, %rd5;
	div.u64 	%rd8, %rd7, %rd6;
	mov.u64 	%rd9, __bpftime_step0_scratch;
	shl.b64 	%rd10, %rd8, 3;
	add.s64 	%rd11, %rd9, %rd10;
	xor.b64 	%rd12, %rd8, 6510615555426900570;
	st.global.u64 	[%rd11], %rd12;
	setp.ne.s32 	%p1, %r4, 0;
	@%p1 bra 	$L__step0_touch_done;
	st.global.u64 	[%rd1], %rd9;
$L__step0_touch_done:
	ret;
}
)PTX";
}

static std::string cread_kernel(uint64_t n)
{
	char b[1024];
	snprintf(b, sizeof(b), R"PTX(
.visible .entry __bpftime_step0_cread(
	.param .u64 __bpftime_step0_cread_param_0
)
{
	.reg .b64 	%%rd<4>;
	ld.param.u64 	%%rd1, [__bpftime_step0_cread_param_0];
	cvta.to.global.u64 	%%rd1, %%rd1;
	ld.const.u64 	%%rd2, [__bpftime_step0_cpad+%llu];
	st.global.u64 	[%%rd1], %%rd2;
	ret;
}
)PTX",
		 (unsigned long long)(n >= 8 ? n - 8 : 0));
	return b;
}

static double ms_since(std::chrono::steady_clock::time_point t)
{
	return std::chrono::duration<double, std::milli>(
		       std::chrono::steady_clock::now() - t)
		.count();
}

static std::string jesc(const std::string &s)
{
	std::string o;
	for (char c : s) {
		if (c == '"' || c == '\\')
			o += '\\', o += c;
		else if (c == '\n')
			o += "\\n";
		else if ((unsigned char)c < 0x20)
			o += ' ';
		else
			o += c;
	}
	return o;
}

int main(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "usage: %s PTX MODE N [--jit] [--info-log FILE]\n", argv[0]);
		return 2;
	}
	std::string path = argv[1], mode = argv[2];
	uint64_t n = strtoull(argv[3], nullptr, 10);
	bool jit = false;
	std::string info_path;
	for (int i = 4; i < argc; i++) {
		if (!strcmp(argv[i], "--jit"))
			jit = true;
		else if (!strcmp(argv[i], "--info-log") && i + 1 < argc)
			info_path = argv[++i];
	}
	std::ifstream ifs(path);
	std::stringstream ss;
	ss << ifs.rdbuf();
	std::string ptx = rewrite_target(ss.str(), "sm_87");
	if (ptx.size() < 100) {
		fprintf(stderr, "cannot read %s\n", path.c_str());
		return 2;
	}
	char decl[256];
	if (mode == "plain")
		snprintf(decl, sizeof(decl), "\n.global .align 8 .b8 %s[%llu];\n", SYM, (unsigned long long)n);
	else if (mode == "visible" || mode == "kernel")
		snprintf(decl, sizeof(decl), "\n.visible .global .align 8 .b8 %s[%llu];\n", SYM, (unsigned long long)n);
	else if (mode == "constpad")
		snprintf(decl, sizeof(decl), "\n.visible .const .align 8 .b8 __bpftime_step0_cpad[%llu];\n", (unsigned long long)n);
	else
		decl[0] = 0;
	ptx += decl;
	if (mode == "kernel")
		ptx += touch_kernel();
	if (mode == "constpad")
		ptx += cread_kernel(n);

	std::string out = "{\"ptx\":\"" + jesc(path) + "\",\"mode\":\"" + mode +
			  "\",\"N\":" + std::to_string(n) + ",\"path\":\"" +
			  (jit ? "cuLibraryLoadData(PTX text, driver JIT)" :
				 "nvPTXCompiler(--gpu-name=sm_87 --verbose -O3) -> cuLibraryLoadData(cubin)") +
			  "\"";
	auto emit = [&](int rc) {
		printf("E8RESULT %s}\n", out.c_str());
		fflush(stdout);
		return rc;
	};
	std::vector<char> image;
	if (!jit) {
		nvPTXCompilerHandle h = nullptr;
		auto t = std::chrono::steady_clock::now();
		nvPTXCompileResult r = nvPTXCompilerCreate(&h, ptx.size(), ptx.c_str());
		const char *opts[] = { "--gpu-name=sm_87", "--verbose", "-O3" };
		if (r == NVPTXCOMPILE_SUCCESS)
			r = nvPTXCompilerCompile(h, 3, opts);
		out += ",\"compile_rc\":" + std::to_string((int)r) +
		       ",\"compile_ms\":" + std::to_string(ms_since(t));
		size_t sz = 0;
		if (h && nvPTXCompilerGetInfoLogSize(h, &sz) == NVPTXCOMPILE_SUCCESS && sz) {
			std::string log(sz, 0);
			nvPTXCompilerGetInfoLog(h, log.data());
			log.resize(strnlen(log.c_str(), log.size())); // the size counts the NUL
			if (!info_path.empty()) {
				std::ofstream o(info_path);
				o << log;
			}
			// constant banks the module and the kernels use
			std::string cm;
			std::istringstream ls(log);
			std::string line;
			while (std::getline(ls, line))
				if (line.find("cmem[") != std::string::npos &&
				    line.find("gmem") != std::string::npos)
					cm += line + "\n";
			out += ",\"module_line\":\"" + jesc(cm) + "\"";
		}
		if (r != NVPTXCOMPILE_SUCCESS) {
			sz = 0;
			if (h && nvPTXCompilerGetErrorLogSize(h, &sz) == NVPTXCOMPILE_SUCCESS && sz) {
				std::string log(sz, 0);
				nvPTXCompilerGetErrorLog(h, log.data());
				log.resize(strnlen(log.c_str(), log.size()));
				out += ",\"compile_error\":\"" + jesc(log.substr(0, 600)) + "\"";
			}
			if (h)
				nvPTXCompilerDestroy(&h);
			return emit(0);
		}
		nvPTXCompilerGetCompiledProgramSize(h, &sz);
		image.resize(sz);
		nvPTXCompilerGetCompiledProgram(h, image.data());
		nvPTXCompilerDestroy(&h);
		out += ",\"cubin_bytes\":" + std::to_string(sz);
		if (!info_path.empty()) {
			std::ofstream o(info_path + ".cubin", std::ios::binary);
			o.write(image.data(), image.size());
		}
	} else {
		image.assign(ptx.begin(), ptx.end());
		image.push_back(0);
	}
	if (mode == "constpad")
		return emit(0); // compile only

	CUresult e = cuInit(0);
	CUdevice dev;
	CUcontext ctx;
	if (e == CUDA_SUCCESS)
		e = cuDeviceGet(&dev, 0);
	if (e == CUDA_SUCCESS)
		e = cuDevicePrimaryCtxRetain(&ctx, dev);
	if (e == CUDA_SUCCESS)
		e = cuCtxSetCurrent(ctx);
	size_t fr = 0, tot = 0;
	cuMemGetInfo(&fr, &tot);
	out += ",\"init_rc\":" + std::to_string((int)e) +
	       ",\"mem_free_before_load\":" + std::to_string(fr);
	if (e != CUDA_SUCCESS)
		return emit(1);
	CUlibrary lib = nullptr;
	auto t = std::chrono::steady_clock::now();
	e = cuLibraryLoadData(&lib, image.data(), nullptr, nullptr, 0, nullptr, nullptr, 0);
	out += ",\"load_rc\":" + std::to_string((int)e) +
	       ",\"load_ms\":" + std::to_string(ms_since(t));
	cuMemGetInfo(&fr, &tot);
	out += ",\"mem_free_after_load\":" + std::to_string(fr);
	if (e != CUDA_SUCCESS) {
		const char *s = nullptr;
		cuGetErrorName(e, &s);
		out += ",\"load_error\":\"" + std::string(s ? s : "?") + "\"";
		return emit(0);
	}
	CUdeviceptr mi = 0;
	size_t misz = 0;
	e = cuLibraryGetGlobal(&mi, &misz, lib, "map_info");
	out += ",\"map_info_rc\":" + std::to_string((int)e) +
	       ",\"map_info_size\":" + std::to_string(misz);
	// with lazy module loading the first symbol lookup loads the module
	cuMemGetInfo(&fr, &tot);
	out += ",\"mem_free_after_map_info\":" + std::to_string(fr);
	CUdeviceptr bogus = 0;
	size_t bsz = 0;
	e = cuLibraryGetGlobal(&bogus, &bsz, lib, "__bpftime_step0_no_such_symbol");
	out += ",\"bogus_rc\":" + std::to_string((int)e);
	if (mode == "base")
		return emit(0);
	CUdeviceptr p = 0;
	size_t psz = 0;
	e = cuLibraryGetGlobal(&p, &psz, lib, SYM);
	out += ",\"global_rc\":" + std::to_string((int)e) +
	       ",\"global_size\":" + std::to_string(psz) +
	       ",\"global_size_ok\":" + std::to_string(e == CUDA_SUCCESS && psz == n) +
	       ",\"global_ptr\":" + std::to_string((unsigned long long)p);
	cuMemGetInfo(&fr, &tot);
	out += ",\"mem_free_after_global\":" + std::to_string(fr);
	if (e != CUDA_SUCCESS)
		return emit(0);
	// host path: memset the whole region, read it all back in chunks
	t = std::chrono::steady_clock::now();
	e = cuMemsetD8(p, 0xA5, n);
	if (e == CUDA_SUCCESS)
		e = cuCtxSynchronize();
	out += ",\"memset_rc\":" + std::to_string((int)e);
	const size_t CH = 64ull << 20;
	std::vector<uint8_t> buf(std::min<size_t>(CH, n));
	uint64_t bad = 0;
	for (size_t off = 0; off < n && e == CUDA_SUCCESS; off += CH) {
		size_t len = std::min(CH, n - off);
		e = cuMemcpyDtoH(buf.data(), p + off, len);
		for (size_t i = 0; i < len && e == CUDA_SUCCESS; i++)
			bad += buf[i] != 0xA5;
	}
	out += ",\"readback_rc\":" + std::to_string((int)e) +
	       ",\"memset_readback_mismatch_bytes\":" + std::to_string(bad) +
	       ",\"host_path_ms\":" + std::to_string(ms_since(t));
	if (mode != "kernel" || e != CUDA_SUCCESS)
		return emit(0);
	// device path: the appended kernel writes through the symbol
	CUkernel k = nullptr;
	CUfunction f = nullptr;
	e = cuLibraryGetKernel(&k, lib, "__bpftime_step0_touch");
	if (e == CUDA_SUCCESS)
		e = cuKernelGetFunction(&f, k);
	CUdeviceptr dout = 0;
	if (e == CUDA_SUCCESS)
		e = cuMemAlloc(&dout, 8);
	const unsigned blocks = 32, tpb = 128, nth = blocks * tpb;
	uint64_t n8 = n / 8;
	void *params[] = { &dout, &n8 };
	if (e == CUDA_SUCCESS)
		e = cuLaunchKernel(f, blocks, 1, 1, tpb, 1, 1, 0, nullptr, params, nullptr);
	if (e == CUDA_SUCCESS)
		e = cuCtxSynchronize();
	uint64_t seen_ptr = 0;
	if (e == CUDA_SUCCESS)
		e = cuMemcpyDtoH(&seen_ptr, dout, 8);
	out += ",\"kernel_rc\":" + std::to_string((int)e) +
	       ",\"kernel_symbol_addr_matches\":" + std::to_string(seen_ptr == (uint64_t)p);
	if (e != CUDA_SUCCESS)
		return emit(0);
	// full readback: written words carry the pattern, every other byte 0xA5
	uint64_t wrong_written = 0, wrong_other = 0, written = 0;
	uint64_t next_g = 0; // positions are increasing in the thread index
	auto pos_of = [&](uint64_t g) { return g * (n8 - 1) / (nth - 1); };
	for (size_t off = 0; off < n && e == CUDA_SUCCESS; off += CH) {
		size_t len = std::min(CH, n - off);
		e = cuMemcpyDtoH(buf.data(), p + off, len);
		for (size_t w = 0; w < len / 8; w++) {
			uint64_t word = off / 8 + w, v;
			memcpy(&v, buf.data() + w * 8, 8);
			while (next_g < nth && pos_of(next_g) < word)
				next_g++;
			bool hit = next_g < nth && pos_of(next_g) == word;
			if (hit) {
				written++;
				wrong_written += v != (word ^ PATTERN);
			} else {
				wrong_other += v != 0xA5A5A5A5A5A5A5A5ull;
			}
		}
	}
	out += ",\"kernel_words_written\":" + std::to_string(written) +
	       ",\"kernel_wrong_written\":" + std::to_string(wrong_written) +
	       ",\"kernel_wrong_other\":" + std::to_string(wrong_other);
	cuMemFree(dout);
	cuLibraryUnload(lib);
	return emit(0);
}

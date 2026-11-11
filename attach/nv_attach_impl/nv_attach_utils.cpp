#include "nv_attach_utils.hpp"
#include "trampoline_ptx.h"
#include <cstring>
#include <iterator>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <openssl/sha.h>
#include <cstdlib>
#include <cuda.h>
#include <mutex>
#include <unistd.h>
namespace fmt_lib = spdlog::fmt_lib;
namespace bpftime
{
namespace attach
{
std::string get_default_trampoline_ptx()
{
	return TRAMPOLINE_PTX;
}
// Read at the start of every probe function (ptxpass_core
// gate_probe_function); 0 turns the probes off.
static const char PROBE_SWITCH_PTX[] =
	"\n.visible .global .align 4 .u32 __bpftime_probes_enabled = 1;\n";

std::string wrap_ptx_with_trampoline(std::string input)
{
	return get_default_trampoline_ptx() + PROBE_SWITCH_PTX + input;
}

std::string rewrite_ptx_target(std::string ptx,
			       const std::string &sm_arch)
{
	if (sm_arch.empty())
		return ptx;

	// Rewrite .version based on SM arch (sm_120+ needs 8.7, sm_100+ needs 8.5)
	int sm_num = (sm_arch.size() > 3) ? std::atoi(sm_arch.c_str() + 3) : 0;
	const char *ptx_ver = (sm_num >= 120) ? "8.7" : (sm_num >= 103) ? "8.8" : (sm_num >= 100) ? "8.5" : nullptr;
	if (ptx_ver) {
		auto vpos = ptx.find(".version");
		if (vpos != std::string::npos) {
			vpos += 8;
			while (vpos < ptx.size() && ptx[vpos] == ' ') vpos++;
			auto vstart = vpos;
			while (vpos < ptx.size() && ptx[vpos] != '\n' && ptx[vpos] != ' ') vpos++;
			if (vpos > vstart)
				ptx.replace(vstart, vpos - vstart, ptx_ver);
		}
	}

	// Rewrite .target
	auto pos = ptx.find(".target");
	if (pos == std::string::npos)
		return ptx;
	pos += strlen(".target");
	while (pos < ptx.size() && (ptx[pos] == ' ' || ptx[pos] == '\t'))
		pos++;
	auto start = pos;
	while (pos < ptx.size() && ptx[pos] != ' ' && ptx[pos] != '\t' &&
	       ptx[pos] != '\n' && ptx[pos] != '\r' && ptx[pos] != ',')
		pos++;
	if (pos <= start)
		return ptx;
	ptx.replace(start, pos - start, sm_arch);
	return ptx;
}

std::string wrap_ptx_with_trampoline_for_sm(std::string input,
					    const std::string &sm_arch)
{
	return rewrite_ptx_target(get_default_trampoline_ptx(), sm_arch) +
	       PROBE_SWITCH_PTX + input;
}

std::string patch_main_from_func_to_entry(std::string result)
{
	const std::string entry_func = ".visible .func bpf_main";

	auto idx = result.find(entry_func);
	SPDLOG_INFO("entry_func ({}) index {}", entry_func, idx);

	if (idx != result.npos) {
		result = result.replace(idx, entry_func.size(),
					".visible .entry bpf_main");
	}
	return result;
}
std::string sha256(const void *data, size_t length)
{
	unsigned char hash[SHA256_DIGEST_LENGTH];
	SHA256((unsigned char *)data, length, hash);

	fmt_lib::memory_buffer buffer;
	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		fmt_lib::format_to(std::back_inserter(buffer), "{:02x}",
				   hash[i]);
	}
	return fmt_lib::to_string(buffer);
}

std::string get_gpu_sm_arch()
{
	// First check environment variable
	const char *sm_arch_env = std::getenv("BPFTIME_SM_ARCH");
	if (sm_arch_env && sm_arch_env[0] != '\0') {
		SPDLOG_DEBUG("Using SM arch from BPFTIME_SM_ARCH: {}",
			     sm_arch_env);
		return sm_arch_env;
	}

	// Detected once and kept here: it used to be stored with setenv in
	// the application's environment. A failure is not kept (the driver may
	// not be initialized yet), and gives no architecture: the old sm_61
	// fallback is refused by CUDA 13 and wrong for any newer device.
	static std::mutex detected_mutex;
	static std::string detected;
	std::lock_guard<std::mutex> guard(detected_mutex);
	if (!detected.empty())
		return detected;

	CUdevice device;
	int major = 0, minor = 0;

	// Prefer current context if one exists; otherwise fall back to device 0.
	CUresult err = cuCtxGetDevice(&device);
	if (err != CUDA_SUCCESS) {
		SPDLOG_DEBUG(
			"cuCtxGetDevice failed with {}, using device 0 for SM arch detection",
			(int)err);
		if (err = cuInit(0); err == CUDA_SUCCESS)
			err = cuDeviceGet(&device, 0);
	}
	if (err == CUDA_SUCCESS)
		err = cuDeviceGetAttribute(
			&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR,
			device);
	if (err == CUDA_SUCCESS)
		err = cuDeviceGetAttribute(
			&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR,
			device);
	if (err != CUDA_SUCCESS) {
		SPDLOG_WARN(
			"Unable to detect the GPU's SM architecture (CUDA error {}); set BPFTIME_SM_ARCH. Kernels stay unpatched",
			(int)err);
		return {};
	}

	detected = "sm_" + std::to_string(major * 10 + minor);
	SPDLOG_INFO("Auto-detected GPU SM arch: {} (compute capability {}.{})",
		    detected, major, minor);
	return detected;
}

uint64_t trampoline_pid_tgid()
{
	const uint64_t tgid = (uint32_t)getpid();
	return (tgid << 32) | tgid;
}

bool write_trampoline_pid_tgid(void *cu_module)
{
	CUdeviceptr ptr;
	size_t size;
	if (cuModuleGetGlobal(&ptr, &size, (CUmodule)cu_module,
			      TRAMPOLINE_PID_TGID_SYMBOL) != CUDA_SUCCESS ||
	    size != sizeof(uint64_t))
		return false;
	const uint64_t value = trampoline_pid_tgid();
	return cuMemcpyHtoD(ptr, &value, sizeof(value)) == CUDA_SUCCESS;
}

} // namespace attach
} // namespace bpftime

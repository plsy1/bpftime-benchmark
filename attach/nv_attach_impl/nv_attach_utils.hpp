#ifndef _NV_ATTACH_UTILS_HPP
#define _NV_ATTACH_UTILS_HPP

#include <cstddef>
#include <cstdint>
#include <dlfcn.h>
#include <string>
namespace bpftime
{
namespace attach
{
extern "C" {
typedef struct {
	int magic;
	int version;
	const unsigned long long *data;
	void *filename_or_fatbins;

} __fatBinC_Wrapper_t;
}
template <class T>
static inline T try_get_original_func(const char *name, T &store)
{
	if (store == nullptr) {
		store = (T)dlsym(RTLD_NEXT, name);
	}
	return store;
}

/**
 * @brief Get the default trampoline ptx object, used for helper operations,
 * which is generated from `test.cu`
 *
 * @return std::string
 */
std::string get_default_trampoline_ptx();
std::string patch_main_from_func_to_entry(std::string);
std::string wrap_ptx_with_trampoline(std::string input);
std::string wrap_ptx_with_trampoline_for_sm(std::string input,
					    const std::string &sm_arch);
std::string sha256(const void *data, size_t length);

/**
 * @brief Rewrite PTX target and version for the given SM architecture.
 * Automatically upgrades PTX version for newer architectures (sm_100+, sm_120+).
 */
std::string rewrite_ptx_target(std::string ptx, const std::string &sm_arch);

/**
 * @brief Get the SM architecture string for the current GPU.
 * First checks BPFTIME_SM_ARCH environment variable, then auto-detects
 * from the current CUDA device (or device 0) if not set; a detected value is
 * kept for the process (the environment is not changed).
 *
 * @return std::string SM architecture string (e.g., "sm_86", "sm_120"), or
 * an empty string when the device cannot be queried (callers then leave the
 * application's kernels unpatched).
 */
std::string get_gpu_sm_arch();

/**
 * @brief bpf_get_current_pid_tgid for this process's GPU threads: the tgid in
 * both halves (a GPU thread is no Linux thread; upstream's GPU programs read
 * the high half). The agent writes it into every module's
 * `__bpftime_pid_tgid` (TRAMPOLINE_PID_TGID_SYMBOL), so the helper needs no
 * host call; the host answers the same for a module it was not written into.
 */
uint64_t trampoline_pid_tgid();
inline constexpr const char *TRAMPOLINE_PID_TGID_SYMBOL = "__bpftime_pid_tgid";
/**
 * @brief Writes trampoline_pid_tgid() into a loaded module (current context).
 * Returns false when the module has no such global or the copy fails; the
 * helper then asks the host.
 */
bool write_trampoline_pid_tgid(void *cu_module);

} // namespace attach
} // namespace bpftime

#endif

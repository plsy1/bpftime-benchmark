#include <sys/auxv.h>
#if defined(__aarch64__)
#include <asm/hwcap.h>
#endif
#include "frida_attach_utils.hpp"
#include "frida_uprobe_attach_impl.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <dlfcn.h>
#include <unordered_map>
#include <utility>
#include <vector>
#include <optional>
#include <spdlog/spdlog.h>
#include <frida-gum.h>
#include <sys/stat.h>
#if __linux__
#include <sys/sysmacros.h>
#endif
#include <unistd.h>
#if __APPLE__
#include <libproc.h>
#endif
// Once per process (exec makes a new one): it was read, and logged at info
// level with a flush, for every probe.
static const std::string &get_executable_path()
{
	static const std::string path = [] {
		char exec_path[PATH_MAX] = { 0 };
#if __linux__
		ssize_t len = readlink("/proc/self/exe", exec_path,
				       sizeof(exec_path) - 1);
		if (len != -1) {
			exec_path[len] = '\0'; // Null-terminate the string
			SPDLOG_DEBUG("Executable path: {}", exec_path);
		} else {
			SPDLOG_ERROR("Error retrieving executable path: {}",
				     errno);
		}
#elif __APPLE__
		pid_t pid = getpid();
		if (proc_pidpath(pid, exec_path, sizeof(exec_path)) > 0) {
			SPDLOG_DEBUG("Executable path: {}", exec_path);
		} else {
			SPDLOG_ERROR("Error retrieving executable path: {}",
				     errno);
		}
#endif
		return std::string(exec_path);
	}();
	return path;
}
namespace bpftime
{
namespace attach
{
namespace
{
std::string unescape_proc_path(std::string path)
{
	std::string result;
	result.reserve(path.size());
	for (size_t i = 0; i < path.size(); i++) {
		if (path[i] == '\\' && i + 3 < path.size() &&
		    path[i + 1] >= '0' && path[i + 1] <= '7' &&
		    path[i + 2] >= '0' && path[i + 2] <= '7' &&
		    path[i + 3] >= '0' && path[i + 3] <= '7') {
			result.push_back((char)((path[i + 1] - '0') * 64 +
					        (path[i + 2] - '0') * 8 +
					        path[i + 3] - '0'));
			i += 3;
		} else {
			result.push_back(path[i]);
		}
	}
	return result;
}
} // namespace

std::optional<std::string>
resolve_mapped_module_path(const std::string_view &module_name)
{
#if __linux__
	std::string name(module_name);
	unsigned pid = 0;
	unsigned long long wanted_start = 0, wanted_end = 0;
	int consumed = 0;
	if (sscanf(name.c_str(), "/proc/%u/map_files/%llx-%llx%n", &pid,
		   &wanted_start, &wanted_end, &consumed) != 3 ||
	    consumed != static_cast<int>(name.size()))
		return name;
	if ((pid_t)pid != getpid() || wanted_start >= wanted_end)
		return {};

	std::ifstream maps("/proc/self/maps");
	std::string line;
	while (std::getline(maps, line)) {
		unsigned long long start, end, offset, inode;
		unsigned dev_major, dev_minor;
		char permissions[5] = {};
		int path_offset = 0;
		if (sscanf(line.c_str(),
			   "%llx-%llx %4s %llx %x:%x %llu %n", &start, &end,
			   permissions, &offset, &dev_major, &dev_minor, &inode,
			   &path_offset) != 7 ||
		    end <= wanted_start || start >= wanted_end)
			continue;
		std::string path = unescape_proc_path(line.substr(path_offset));
		struct stat st = {};
		if (path.empty() || path.front() != '/' ||
		    path.ends_with(" (deleted)") || stat(path.c_str(), &st) != 0 ||
		    (inode != 0 && st.st_ino != inode) ||
		    major(st.st_dev) != dev_major || minor(st.st_dev) != dev_minor)
			return {};
		return path;
	}
	return {};
#else
	return std::string(module_name);
#endif
}

#if __linux__
namespace
{
struct exec_vma {
	uintptr_t start, end, pgoff;
};

void *translate_through_vma(const exec_vma &vma, uintptr_t file_offset)
{
	if (file_offset < vma.pgoff || file_offset - vma.pgoff >= vma.end - vma.start)
		return nullptr;
	return reinterpret_cast<void *>(vma.start + (file_offset - vma.pgoff));
}

// Mirror Linux kernel/events/uprobes.c offset_to_vaddr(): find the executable
// file-backed VMA for `target_path` in /proc/self/maps covering `file_offset`,
// and translate to vma_start + (file_offset - vma_pgoff). The VMA used is
// stored in `found`.
void *resolve_exec_vma_by_file_offset(const std::string &target_path,
				      uintptr_t file_offset, exec_vma *found)
{
	struct stat target_st = {};
	const bool have_target_st = !target_path.empty() &&
				    stat(target_path.c_str(), &target_st) == 0;
	std::ifstream maps("/proc/self/maps");
	std::string line;
	while (std::getline(maps, line)) {
		unsigned long long start = 0, end = 0, pgoff = 0, inode = 0;
		unsigned dev_major = 0, dev_minor = 0;
		char perms[5] = {};
		int path_offset = 0;
		if (sscanf(line.c_str(), "%llx-%llx %4s %llx %x:%x %llu %n",
			   &start, &end, perms, &pgoff, &dev_major, &dev_minor,
			   &inode, &path_offset) != 7)
			continue;
		if (perms[2] != 'x' || end <= start)
			continue;
		if (file_offset < pgoff || file_offset - pgoff >= end - start)
			continue;
		std::string path = unescape_proc_path(line.substr(path_offset));
		if (path.empty() || path.front() != '/' ||
		    path.ends_with(" (deleted)"))
			continue;
		bool matched = (path == target_path);
		if (!matched && have_target_st && inode != 0 &&
		    inode == target_st.st_ino &&
		    dev_major == major(target_st.st_dev) &&
		    dev_minor == minor(target_st.st_dev)) {
			matched = true;
		}
		if (!matched && have_target_st) {
			std::error_code ec;
			if (std::filesystem::equivalent(path, target_path,
							ec) &&
			    !ec)
				matched = true;
		}
		if (matched) {
			if (found != nullptr)
				*found = { static_cast<uintptr_t>(start),
					   static_cast<uintptr_t>(end),
					   static_cast<uintptr_t>(pgoff) };
			return reinterpret_cast<void *>(
				static_cast<uintptr_t>(start) +
				(file_offset - static_cast<uintptr_t>(pgoff)));
		}
	}
	return nullptr;
}
} // namespace
#endif

namespace
{
// What a module's lookups found, kept while a module is still loaded there
// (dladdr finds the same object base). Every probe looked its module up again
// (gum_module_load and Gum's module enumeration, with the executable path and
// a flushed log line, and a scan of /proc/self/maps): 0.42 ms per probe at
// every process start, 0.44 s with 1,000 probes
// (uprobe/more-dims-20261106, n4-many-probes).
struct module_cache_entry {
	void *base = nullptr;
#if __linux__
	// Executable VMAs found by file offset, each with its object's base.
	std::vector<std::pair<exec_vma, void *> > vmas;
#endif
};

bool still_loaded_at(const void *address, const void *object_base)
{
	Dl_info info{};
	return dladdr(address, &info) != 0 && info.dli_fbase == object_base;
}
} // namespace

void *
resolve_function_addr_by_module_offset(const std::string_view &module_name,
				       uintptr_t func_offset)
{
	static std::mutex cache_mutex;
	static std::unordered_map<std::string, module_cache_entry> cache;
	const std::string key(module_name);
	void *cached_base = nullptr;
	{
		std::lock_guard<std::mutex> guard(cache_mutex);
		if (auto itr = cache.find(key); itr != cache.end()) {
			auto &entry = itr->second;
#if __linux__
			for (auto vma = entry.vmas.begin();
			     vma != entry.vmas.end();) {
				if (!still_loaded_at((void *)vma->first.start,
						     vma->second)) {
					vma = entry.vmas.erase(vma);
					continue;
				}
				// The same translation as an uncached lookup
				// (#904); a hit is inside a mapped VMA.
				if (void *addr = translate_through_vma(
					    vma->first, func_offset))
					return addr;
				++vma;
			}
#endif
			if (entry.base != nullptr &&
			    still_loaded_at(entry.base, entry.base))
				cached_base = entry.base;
			else
				entry.base = nullptr;
		}
	}
	const auto &exec_path = get_executable_path();
	std::error_code ec;
	// Resolve the executable alias the same way for the base and its VMAs.
	const std::string resolved_name =
		(module_name.empty() ||
		 std::filesystem::equivalent(module_name, exec_path, ec))
			? "" : std::string(module_name);
#if __linux__
	const std::string &target_path =
		resolved_name.empty() ? exec_path : resolved_name;
	exec_vma found{};
	if (void *vma_addr = resolve_exec_vma_by_file_offset(
		    target_path, func_offset, &found)) {
		// Kept only for mappings of a loaded object (not, for
		// example, a file mapped by hand), as the base is.
		Dl_info info{};
		if (dladdr((void *)found.start, &info) != 0 &&
		    info.dli_fbase != nullptr) {
			std::lock_guard<std::mutex> guard(cache_mutex);
			cache[key].vmas.emplace_back(found, info.dli_fbase);
		}
		return vma_addr;
	}
#endif
	void *module_base_addr = cached_base;
	if (module_base_addr == nullptr)
		module_base_addr = get_module_base_addr(resolved_name.c_str());
	if (!module_base_addr) {
		// It's not a bug, it might be attach to a unrelated process
		// when using the LD_PRELOAD
		SPDLOG_INFO("Failed to find module base address for {}",
			    module_name);
		return nullptr;
	}
	if (cached_base == nullptr) {
		std::lock_guard<std::mutex> guard(cache_mutex);
		cache[key].base = module_base_addr;
	}

	// Reject both integer overflow and unmapped gaps. Missing mappings fail
	// closed: an unchecked address must never reach Frida's trampoline writer.
	const auto base = reinterpret_cast<uintptr_t>(module_base_addr);
	if (func_offset > UINTPTR_MAX - base ||
	    !module_contains_address(resolved_name, base + func_offset)) {
		SPDLOG_ERROR("Module offset 0x{:x} is not mapped in {}",
			     func_offset, module_name);
		return nullptr;
	}
	return reinterpret_cast<void *>(base + func_offset);
}

// The symbol helpers below may run before any frida_attach_impl exists (ufunc
// resolution, tests); gum's lookups crash before gum_init_embedded
// (gum_find_function -> g_hash_table_new_full -> g_slice_alloc). Initializing
// again is harmless.
static void ensure_gum_initialized()
{
	static std::once_flag once;
	std::call_once(once, [] { gum_init_embedded(); });
}

void *find_function_addr_by_name(const char *name)
{
	ensure_gum_initialized();
	if (auto ptr = gum_find_function(name); ptr)
		return ptr;
	if (auto ptr = (void *)gum_module_find_export_by_name(nullptr, name);
	    ptr)
		return ptr;
	return nullptr;
}

void *get_module_base_addr(const char *module_name)
{
	ensure_gum_initialized();
	gum_module_load(module_name, nullptr);
	return (void *)gum_module_find_base_address(module_name);
}

namespace {
struct ExtentAccumulator {
	uintptr_t begin = UINTPTR_MAX;
	uintptr_t end = 0;
};

gboolean accumulate_range(const GumRangeDetails *details, gpointer user_data)
{
	auto *acc = static_cast<ExtentAccumulator *>(user_data);
	const auto base = (uintptr_t)details->range->base_address;
	const auto range_end = base + (uintptr_t)details->range->size;
	if (base < acc->begin)
		acc->begin = base;
	if (range_end > acc->end)
		acc->end = range_end;
	return TRUE;
}

struct AddressLookup {
	uintptr_t address;
	bool found = false;
};

gboolean find_mapped_address(const GumRangeDetails *details, gpointer user_data)
{
	auto *lookup = static_cast<AddressLookup *>(user_data);
	const auto start = (uintptr_t)details->range->base_address;
	if (lookup->address >= start &&
	    lookup->address - start < (uintptr_t)details->range->size) {
		lookup->found = true;
		return FALSE;
	}
	return TRUE;
}

} // namespace

// gum's module lookup matches a full path for the base address but, for range
// enumeration, only the executable name (the basename) is matched; a full path
// yields zero ranges. Try the given name and then the basename so a path from
// an attach spec still resolves.
bool module_contains_address(std::string_view module_name, uintptr_t address)
{
	ensure_gum_initialized();
	AddressLookup lookup{address};
	const std::string name(module_name);
	gum_module_enumerate_ranges(name.c_str(), GUM_PAGE_READ,
				    find_mapped_address, &lookup);
	if (lookup.found)
		return true;

	const auto slash = name.find_last_of('/');
	if (slash == std::string::npos || slash + 1 >= name.size())
		return false;
	gum_module_enumerate_ranges(name.c_str() + slash + 1, GUM_PAGE_READ,
				    find_mapped_address, &lookup);
	return lookup.found;
}

// Returns the span covering the module's mapped ranges (possibly with gaps),
// or 0 when the ranges cannot be determined.
uintptr_t module_mapped_extent(const std::string_view &module_name)
{
	ensure_gum_initialized();
	const std::string name(module_name);
	ExtentAccumulator acc;
	gum_module_enumerate_ranges(name.c_str(), GUM_PAGE_READ,
				    accumulate_range, &acc);
	if (acc.end <= acc.begin)
		return 0;
	return acc.end - acc.begin;
}
int from_cb_idx_to_attach_type(int idx)
{
	switch (idx) {
	case ATTACH_UPROBE_INDEX:
		return ATTACH_UPROBE;
	case ATTACH_UPROBE_OVERRIDE_INDEX:
		return ATTACH_UPROBE_OVERRIDE;
	case ATTACH_URETPROBE_INDEX:
		return ATTACH_URETPROBE;
	default:
		SPDLOG_ERROR("Unreachable branch reached!");
		return -1;
	}
	return 0;
}
} // namespace attach
} // namespace bpftime

extern "C" uint64_t bpftime_get_func_ret(uint64_t ctx, uint64_t *value,
					 uint64_t, uint64_t, uint64_t)
{
	GumInvocationContext *gum_ctx =
		gum_interceptor_get_current_invocation();
	if (gum_ctx == NULL) {
		return -EOPNOTSUPP;
	}
	// ignore ctx;
	*value = (uint64_t)gum_invocation_context_get_return_value(gum_ctx);
	return 0;
}

extern "C" uint64_t bpftime_get_func_arg(uint64_t ctx, uint32_t n,
					 uint64_t *value, uint64_t, uint64_t)
{
	GumInvocationContext *gum_ctx =
		gum_interceptor_get_current_invocation();
	if (gum_ctx == NULL) {
		return -EINVAL;
	}
	// ignore ctx;
	*value = (uint64_t)gum_cpu_context_get_nth_argument(
		gum_ctx->cpu_context, n);
	return 0;
}

extern "C" uint64_t bpftime_get_retval(uint64_t, uint64_t, uint64_t, uint64_t,
				       uint64_t)
{
	GumInvocationContext *gum_ctx =
		gum_interceptor_get_current_invocation();
	if (gum_ctx == NULL) {
		return -EOPNOTSUPP;
	}
	return (uintptr_t)gum_invocation_context_get_return_value(gum_ctx);
}

// Strip the pointer authentication code of a return address (libc and the
// agent sign theirs); gum_strip_code_address does not strip on Linux, and
// frames came out as 0x0005ffff... XPACI is undefined without PAC, so check
// the hwcap; without PAC nothing is signed. (A naked asm function here lost
// its argument to interprocedural optimization.)
uint64_t bpftime::attach::strip_code_pointer_auth(uint64_t addr)
{
#if defined(__aarch64__)
	static const bool has_pac = (getauxval(AT_HWCAP) & HWCAP_PACA) != 0;
	if (!has_pac)
		return addr;
	register uint64_t x0 asm("x0") = addr;
	asm(".inst 0xdac143e0" : "+r"(x0)); // xpaci x0
	return x0;
#else
	return addr;
#endif
}


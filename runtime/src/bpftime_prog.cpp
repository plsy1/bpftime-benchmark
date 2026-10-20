/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#include "bpftime.hpp"
#include "base_attach_impl.hpp"
#include "bpftime_config.hpp"
#include "bpftime_helper_group.hpp"
#include "bpftime_internal.h"
#include "bpftime_vm_compat.hpp"
#include "ebpf-vm.h"
#ifdef BPFTIME_ENABLE_CUDA_ATTACH
#include "nvPTXCompiler.h"
#endif
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include "bpftime_vm_compat.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <set>
#include <climits>
#include <linux/bpf.h>
#include <bpftime_shm_internal.hpp>
#include <bpf_map/array_map_binding.hpp>

#define NVPTXCOMPILER_SAFE_CALL(x)                                             \
	do {                                                                   \
		nvPTXCompileResult result = x;                                 \
		if (result != NVPTXCOMPILE_SUCCESS) {                          \
			SPDLOG_ERROR("error: {} failed with error code {}",    \
				     #x, (int)result);                         \
			return {};                                             \
		}                                                              \
	} while (0)

using namespace std;
namespace bpftime
{
#ifdef BPFTIME_ENABLE_CUDA_ATTACH
std::optional<std::vector<char>> compile_ptx_to_elf(const std::string &ptx_code,
						    const char *cpu_target)
{
	unsigned int minor_version, major_version;
	NVPTXCOMPILER_SAFE_CALL(
		nvPTXCompilerGetVersion(&major_version, &minor_version));
	SPDLOG_INFO("ptx compiler version: {}.{}", major_version,
		    minor_version);
	SPDLOG_DEBUG("Starting compiling with ptx code: \n{}", ptx_code);
	nvPTXCompilerHandle handle = nullptr;
	NVPTXCOMPILER_SAFE_CALL(nvPTXCompilerCreate(
		&handle, (size_t)ptx_code.size(), ptx_code.c_str()));
	const auto deleter = [](struct nvPTXCompiler *ptr) {
		if (auto err = nvPTXCompilerDestroy(&ptr);
		    err != NVPTXCOMPILE_SUCCESS) {
			SPDLOG_ERROR("Unable to destroy compiler: {}",
				     (int)err);
		}
	};
	std::unique_ptr<struct nvPTXCompiler, decltype(deleter)> compiler(
		handle, deleter);
	std::string opt1 = "--gpu-name=";
	opt1 += cpu_target;
	const char *compile_options[] = { opt1.c_str(), "--verbose" };

	if (auto result = nvPTXCompilerCompile(compiler.get(),
					       std::size(compile_options),
					       compile_options);
	    result != NVPTXCOMPILE_SUCCESS) {
		size_t sz;
		NVPTXCOMPILER_SAFE_CALL(
			nvPTXCompilerGetErrorLogSize(compiler.get(), &sz));
		std::string error(sz, 0);
		if (sz != 0) {
			NVPTXCOMPILER_SAFE_CALL(nvPTXCompilerGetErrorLog(
				compiler.get(), (char *)error.c_str()));
			SPDLOG_ERROR("Unable to compile ptx: {}", error);
			return {};
		}
	}
	size_t elf_size;
	NVPTXCOMPILER_SAFE_CALL(
		nvPTXCompilerGetCompiledProgramSize(compiler.get(), &elf_size));
	std::vector<char> elf_binary(elf_size, 0);
	NVPTXCOMPILER_SAFE_CALL(nvPTXCompilerGetCompiledProgram(
		compiler.get(), (void *)elf_binary.data()));

	{
		size_t info_size;

		NVPTXCOMPILER_SAFE_CALL(nvPTXCompilerGetInfoLogSize(
			compiler.get(), &info_size));
		std::string info(info_size, 0);
		if (info_size != 0) {
			NVPTXCOMPILER_SAFE_CALL(nvPTXCompilerGetInfoLog(
				compiler.get(), (char *)info.c_str()));
			SPDLOG_INFO("Compiler log: {}", info);
		}
	}

	return elf_binary;
}
#endif
thread_local std::optional<uint64_t> current_thread_bpf_cookie;
thread_local std::optional<uint64_t> current_thread_tail_call_ret;

namespace {
thread_local array_jit_execution_scope *active_array_scope = nullptr;
}

// Only bound programs enter this scope. Native code has no per-lookup guard.
// The slow helper also uses the pinned identity, not a possibly reused FD.
struct array_jit_execution_scope {
	const bpftime_prog &prog;
	array_jit_execution_scope *previous;
	std::map<uint64_t, std::shared_ptr<array_map_binding>> dynamic_arrays;
	explicit array_jit_execution_scope(const bpftime_prog &p)
		: prog(p), previous(active_array_scope)
	{
		active_array_scope = this;
	}
	~array_jit_execution_scope() { active_array_scope = previous; }
	static uint64_t lookup(uint64_t fd, uint64_t key, uint64_t, uint64_t, uint64_t)
	{
		// The JIT uses the already-invalid FD -1 only after proving that a
		// registered ordinary-array index is out of range. Its standard
		// outcome is ENOENT/null, without consulting any mutable FD table.
		if (fd == UINT64_MAX)
			return reinterpret_cast<uint64_t>(detail::array_lookup_enoent());
		auto *scope = active_array_scope;
		if (!scope) {
			errno = ENOENT;
			return 0;
		}
		const auto &bindings = scope->prog.bound_arrays;
		auto found = bindings.find(fd);
		array_map_binding *binding = nullptr;
		if (found != bindings.end()) {
			binding = found->second.get(); // The executing program owns it.
		} else {
			// Dynamic handles are not inlined. Keep a temporary reference
			// through the entire invocation, including uses of its result.
			auto &dynamic = scope->dynamic_arrays;
			auto it = dynamic.find(fd);
			if (it == dynamic.end() && fd <= INT_MAX) {
				auto acquired = array_map_binding::acquire(scope->prog.bound_mapping,
					scope->prog.bound_manager, static_cast<int>(fd));
				if (acquired)
					it = dynamic.emplace(fd, std::move(acquired)).first;
			}
			if (it != dynamic.end())
				binding = it->second.get();
		}
		if (!binding) {
			errno = ENOENT;
			return 0;
		}
		return reinterpret_cast<uint64_t>(binding->implementation()->elem_lookup(
			reinterpret_cast<const void *>(key)));
	}
};

int bpftime_prog::bind_array_maps(bpftime_shm &shm)
{
#if BPFTIME_ENABLE_MPK
	return 0; // Retained mappings have not been audited with MPK transitions.
#endif
	if (is_cuda() || std::string_view(vm_backend_name()) != "llvm")
		return 0;
	auto helper = raw_helpers.find(BPF_FUNC_map_lookup_elem);
	if (helper == raw_helpers.end() || helper->second !=
	    reinterpret_cast<void *>(bpftime_array_map_lookup_elem_helper))
		return 0; // Never bypass a generic/custom/diagnostic helper.
	std::set<int> fds;
	bool has_lookup = false;
	for (size_t i = 0; i < insns.size(); ++i) {
		const auto &inst = insns[i];
		if (inst.code == 0x18) {
			if (i + 1 == insns.size())
				return 0;
			if (inst.src_reg == BPF_PSEUDO_MAP_FD && inst.imm >= 0)
				fds.insert(inst.imm);
			else if (inst.src_reg != 0)
				return 0; // Direct map-value, index and other relocations.
			++i;
		} else if (inst.code == 0x8d) {
			return 0; // Unknown indirect helper implementation.
		} else if (inst.code == 0x85) {
			if (inst.src_reg == 1)
				continue; // BPF-to-BPF call within the same program.
			if (inst.src_reg != 0 || inst.imm != BPF_FUNC_map_lookup_elem)
				return 0; // Other map operations/backends retain their path.
			has_lookup = true;
		}
	}
	if (!has_lookup || fds.empty())
		return 0;
	auto memory = shm.retain_mapping();
	if (!memory->find<boost::interprocess::interprocess_recursive_mutex>(
		"bpftime_map_binding_mutex_v1").first)
		return 0; // Legacy segments do not advertise synchronized lifetime support.
	map_binding_guard guard(map_binding_mutex(*memory));
	std::map<uint64_t, std::shared_ptr<array_map_binding>> bindings;
	for (int fd : fds) {
		auto binding = array_map_binding::acquire(memory, shm.get_manager(), fd);
		if (!binding)
			return 0;
		auto *impl = binding->implementation();
		if (!impl->get_value_size() || !impl->get_max_entries() ||
		    uint64_t(impl->get_value_size()) * impl->get_max_entries() > UINT32_MAX)
			return 0; // The existing array implementation uses 32-bit offsets.
		bindings.emplace(fd, std::move(binding));
	}
	for (const auto &[fd, binding] : bindings) {
		auto *impl = binding->implementation();
		int result = ebpf_register_array_map(vm, fd,
			reinterpret_cast<uint64_t>(impl->get_raw_data()), sizeof(uint32_t),
			impl->get_value_size(), impl->get_value_size(), impl->get_max_entries());
		if (result < 0)
			return result; // Abort this program load, never use a partial binding.
	}
	bound_arrays = std::move(bindings);
	bound_mapping = std::move(memory);
	bound_manager = shm.get_manager();
	SPDLOG_INFO("[array-jit-inline] program={} retained {} maps", name, bound_arrays.size());
	return 0;
}

bpftime_prog::bpftime_prog(const struct ebpf_inst *insn, size_t insn_cnt,
			   const char *name)
	: name(name)
{
	SPDLOG_DEBUG("Creating bpftime_prog with name {}", name);
	insns.assign(insn, insn + insn_cnt);
	const char *vm_name = bpftime::bpftime_get_runtime_config().get_vm_name();
	std::string vm_name_str = (std::string)vm_name;

	if (vm_name_str == "llvm") {
		SPDLOG_DEBUG("Creating vm with name {}", vm_name_str);
	} else if (vm_name_str == "ubpf") {
		SPDLOG_DEBUG("Creating vm with name {}", vm_name_str);
	} else {
		SPDLOG_DEBUG("Trying enabling non-builtin vm {}", vm_name_str);
	}

	vm = ebpf_create(vm_name);
	// Disable bounds check because we have no implementation yet
	// ebpf_toggle_bounds_check(vm, false);
	ebpf_set_lddw_helpers(vm, map_ptr_by_fd, nullptr, map_val, nullptr,
			      nullptr);
}

bpftime_prog::bpftime_prog(const struct ebpf_inst *insn, size_t insn_cnt,
			   const char *name, runtime_config config)
	: name(name)
{
	// BPFtime_prog relies on the global shared memory being properly
	// initialized to function.
	SPDLOG_DEBUG("Creating bpftime_prog with name {}", name);
	insns.assign(insn, insn + insn_cnt);
	bpftime::bpftime_set_runtime_config(std::move(config));
	const char *vm_name = bpftime::bpftime_get_runtime_config().get_vm_name();
	std::string vm_name_str = (std::string)vm_name;

	if (vm_name_str == "llvm") {
		SPDLOG_DEBUG("Creating vm with name {}", vm_name_str);
	} else if (vm_name_str == "ubpf") {
		SPDLOG_DEBUG("Creating vm with name {}", vm_name_str);
	} else {
		SPDLOG_DEBUG("Trying enabling non-builtin vm {}", vm_name_str);
	}

	vm = ebpf_create(vm_name);
	// Disable bounds check because we have no implementation yet
	// ebpf_toggle_bounds_check(vm, false);
	if (!is_cuda()) {
		ebpf_set_lddw_helpers(vm, map_ptr_by_fd, nullptr, map_val,
				      nullptr, nullptr);
	} else {
		SPDLOG_INFO("Do not set lddw helpers due to cuda program");
		ebpf_set_lddw_helpers(vm, map_ptr_by_fd, nullptr, map_val,
				      nullptr, nullptr);
	}
}

bpftime_prog::~bpftime_prog()
{
	ebpf_unload_code(vm);
	ebpf_destroy(vm);
}

int bpftime_prog::bpftime_prog_load(bool jit)
{
	if (array_jit_failed)
		return -EINVAL;
	if (!bound_arrays.empty())
		return -EBUSY; // Do not mutate bindings of already published code.
	signal_ready = false;
	loaded_from_aot = false;
	program_loaded = false;
	int res = -1;
	// A previously loaded interpreter/fallback program already has its helper
	// slot fixed. Only the first deferred registration can select a binding.
	if (jit && array_jit_requested && pending_array_lookup) {
		res = bind_array_maps(shm_holder.global_shared_memory);
		if (res < 0) {
			array_jit_failed = true;
			return res;
		}
	}
	if (pending_array_lookup) {
		void *helper = bound_arrays.empty() ? pending_array_lookup :
			reinterpret_cast<void *>(array_jit_execution_scope::lookup);
		const char *helper_name = bound_arrays.empty() ? "bpf_map_lookup_elem" :
			"bpftime_bound_array_lookup_v1";
		res = ebpf_register(vm, BPF_FUNC_map_lookup_elem, helper_name, helper);
		if (res < 0) {
			array_jit_failed = true;
			return res;
		}
		pending_array_lookup = nullptr;
	}

	SPDLOG_DEBUG("Load insn cnt {}", insns.size());
	res = ebpf_load(vm, insns.data(),
			insns.size() * sizeof(struct ebpf_inst), &errmsg);
	if (res < 0) {
		SPDLOG_ERROR("Failed to load insn: {}", errmsg);
		return res;
	}
	if (is_cuda()) {
		// SPDLOG_INFO("Compiling CUDA program");

		// this->bpftime_prog_register_raw_helper(bpftime_helper_info{
		// 	.index = 501,
		// 	.name = "puts",
		// 	.fn = (void *)&puts,
		// });

		// ptx_code = ((struct ebpf_vm *)vm)
		// 		   ->vm_instance->generate_ptx("sm_60");

		// if (!ptx_code.has_value()) {
		// 	throw std::runtime_error("Failed to generate ptx code");
		// }
		// *ptx_code =
		// 	wrap_ptx_with_trampoline(patch_helper_names_and_header(
		// 		patch_main_from_func_to_entry(*ptx_code)));
		// cuda_elf_binary = compile_ptx_to_elf(*ptx_code, "sm_60");
		// if (!cuda_elf_binary.has_value()) {
		// 	throw std::runtime_error("unable to compile ptx code");
		// }
	} else {
		if (jit) {
			ebpf_jit_fn jit_fn = ebpf_compile(vm, &errmsg);
			if (jit_fn == NULL) {
				SPDLOG_WARN("JIT compilation failed ({}), falling back to interpreter", errmsg);
				jitted = false;
			} else {
				jitted = true;
				fn = jit_fn;
				// POC-only diagnostic: dump a readable executable mapping
				// window beginning at the ACTUAL JIT entry, not an AOT size.
				// The window can include padding/other functions after ours.
				if (const char *dir = std::getenv("BPFTIME_ARRAY_JIT_POC_DUMP_DIR")) {
					const auto address = reinterpret_cast<uintptr_t>(fn);
					std::ifstream maps("/proc/self/maps");
					std::string line;
					while (std::getline(maps, line)) {
						unsigned long begin = 0, end = 0;
						char perms[5] = {};
						if (std::sscanf(line.c_str(), "%lx-%lx %4s", &begin, &end, perms) == 3 &&
						    begin <= address && address < end && perms[0] == 'r' && perms[2] == 'x') {
							auto length = std::min<size_t>(4096, end - address);
							std::string label = name;
							std::replace(label.begin(), label.end(), '/', '_');
							std::string path = std::string(dir) + "/" + label + ".bin";
							std::ofstream file(path, std::ios::binary);
							file.write(reinterpret_cast<const char *>(address), length);
							SPDLOG_INFO("[array-jit-poc] dump={} address={:x} readable_window_bytes={}", path, address, length);
							break;
						}
					}
				}
			}
		} else {
			jitted = false;
		}
	}

	program_loaded = true;
	return 0;
}

int bpftime_prog::bpftime_prog_unload()
{
	signal_ready = false;
	program_loaded = false;
	if (jitted) {
		// ignore for jit
		return 0;
	}
	ebpf_unload_code(vm);
	return 0;
}

int bpftime_prog::bpftime_prog_exec(void *memory, size_t memory_size,
				    uint64_t *return_val) const
{
	if (array_jit_requested && (!program_loaded || array_jit_failed)) {
		if (return_val)
			*return_val = 0;
		return -EINVAL;
	}
	if (!bound_arrays.empty() &&
	    !bound_arrays.begin()->second->belongs_to_this_process()) {
		if (return_val)
			*return_val = 0;
		return -ENOTSUP; // Reload in the child; inherited references are borrowed.
	}
	if (attach::current_signal_callback) {
		if (!signal_ready || !jitted || !return_val || !bound_arrays.empty())
			return -ENOTSUP;
		// No VM dispatcher, lazy compilation, logging, dynamic TLS or
		// shared-memory protection helpers may run from SIGTRAP.
		*return_val = fn(memory, memory_size);
		return 0;
	}
	if (is_cuda()) {
		throw std::runtime_error("Unable to execute CUDA program");
	}
	uint64_t val = 0;
	int res = 0;
	std::optional<array_jit_execution_scope> binding_scope;
	if (!bound_arrays.empty())
		binding_scope.emplace(*this);
	// set share memory read and write able
	bpftime_protect_disable();
	SPDLOG_DEBUG(
		"Calling bpftime_prog::bpftime_prog_exec, memory={:x}, memory_size={}, return_val={:x}, prog_name={}",
		(uintptr_t)memory, memory_size, (uintptr_t)return_val,
		this->name);
	if (jitted) {
		SPDLOG_DEBUG("Directly call jitted function at {:x}",
			     (uintptr_t)fn);
		val = fn(memory, memory_size);
	} else {
		SPDLOG_DEBUG("Running using ebpf_exec");
		res = ebpf_exec(vm, memory, memory_size, &val);
		if (res < 0) {
			SPDLOG_ERROR("ebpf_exec returned error: {}", res);
		}
	}
	*return_val = current_thread_tail_call_ret.value_or(val);
	current_thread_tail_call_ret.reset();
	// set share memory read only
	bpftime_protect_enable();
	return res;
}

int bpftime_prog::bpftime_prog_register_raw_helper(
	struct bpftime_helper_info info)
{
	if (!bound_arrays.empty() && info.index == BPF_FUNC_map_lookup_elem)
		return -EBUSY;
	if (array_jit_requested && info.index == BPF_FUNC_map_lookup_elem &&
	    info.fn == reinterpret_cast<void *>(bpftime_array_map_lookup_elem_helper)) {
		if (pending_array_lookup || raw_helpers.contains(info.index))
			return -EEXIST;
		// LLVM helper slots are immutable once registered. Select exactly once
		// at load time, after eligibility and map ownership are established.
		pending_array_lookup = info.fn;
		raw_helpers[info.index] = info.fn;
		signal_ready = false;
		signal_safe_helpers[info.index] = helper_is_async_signal_safe(info.index, info.fn);
		return 0;
	}
	if (pending_array_lookup && info.index == BPF_FUNC_map_lookup_elem)
		return -EEXIST;
	int result = ebpf_register(vm, info.index, info.name.c_str(), info.fn);
	if (result == 0) {
		raw_helpers[info.index] = info.fn;
		signal_ready = false;
		signal_safe_helpers[info.index] = helper_is_async_signal_safe(info.index, info.fn);
	}
	return result;
}

int bpftime_prog::prepare_for_signal_execution()
{
	if (!bound_arrays.empty())
		return -ENOTSUP; // TLS scope and reference accounting are not signal-safe.
	// A program can be linked to several live sites. Once prepared, adding
	// another link must not mutate state read by an existing signal callback.
	if (signal_ready)
		return 0;
	if (!program_loaded)
		return -EINVAL;
#if BPFTIME_ENABLE_MPK
	// The MPK access transition has not been audited for signal callbacks.
	return -ENOTSUP;
#endif
	if (is_cuda() || loaded_from_aot)
		return -ENOTSUP;
	const char *backend = ebpf_get_vm_name(vm);
	if (strcmp(backend, "llvm") != 0 && strcmp(backend, "ubpf") != 0)
		return -ENOTSUP;
	for (size_t i = 0; i < insns.size(); ++i) {
		const auto &inst = insns[i];
		if (inst.code == 0x18) { // LDDW's second word is data.
			++i;
			continue;
		}
		if (inst.code == 0x8d) // Indirect CALLX cannot be audited here.
			return -ENOTSUP;
		if (inst.code != 0x85)
			continue;
		if (inst.src_reg == 1) {
			// Only LLVM's compatibility layer distinguishes local calls
			// from helper calls when loading bytecode.
			if (strcmp(backend, "llvm") == 0)
				continue;
			return -ENOTSUP;
		}
		auto helper = signal_safe_helpers.find((unsigned)inst.imm);
		if (inst.src_reg != 0 || helper == signal_safe_helpers.end() ||
		    !helper->second) {
			SPDLOG_ERROR("Trap attach rejected for {}: helper {} is not async-signal-safe",
				     name, inst.imm);
			return -ENOTSUP;
		}
	}
	if (!jitted) {
		auto compiled = ebpf_compile(vm, &errmsg);
		if (!compiled)
			return -ENOTSUP;
		fn = compiled;
		jitted = true;
	}
	signal_ready = true;
	return 0;
}

int bpftime_prog::load_aot_object(const std::vector<uint8_t> &buf)
{
	if (!bound_arrays.empty() || array_jit_failed)
		return -ENOTSUP; // Absolute process-local bindings cannot be serialized.
	signal_ready = false;
	loaded_from_aot = true;
	ebpf_jit_fn res = ebpf_load_aot_object(vm, buf.data(), buf.size());
	if (res == nullptr) {
		SPDLOG_ERROR("Failed to load aot object");
		return -1;
	}
	fn = res;
	jitted = true;
	return 0;
}

} // namespace bpftime

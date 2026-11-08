/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#include "bpftime.hpp"
#include "bpftime_ratelimited_log.hpp"
#include "base_attach_impl.hpp"
#include "bpftime_config.hpp"
#include "bpftime_helper_group.hpp"
#include "bpftime_internal.h"
#include "bpftime_vm_compat.hpp"
#include "ebpf-vm.h"
#include "bpftime_direct_map_helpers.hpp"
#include "bpftime_prog_reachability.hpp"
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
#include <tuple>
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
#include <bpf_map/map_production_ab.hpp>

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
using map_helper_fn = uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
bool standard_lookup(void *fn)
{
	return fn == reinterpret_cast<void *>(bpftime_map_lookup_elem_helper) ||
	       fn == reinterpret_cast<void *>(bpftime_array_map_lookup_elem_helper) ||
	       fn == reinterpret_cast<void *>(bpftime_percpu_array_map_lookup_elem_helper) ||
	       fn == reinterpret_cast<void *>(bpftime_hash_map_lookup_elem_helper) ||
	       fn == reinterpret_cast<void *>(bpftime_percpu_hash_map_lookup_elem_helper);
}
bool standard_mutation(unsigned index, void *fn)
{
	if (index == BPF_FUNC_map_delete_elem)
		return fn == reinterpret_cast<void *>(bpftime_map_delete_elem_helper) ||
		       fn == reinterpret_cast<void *>(bpftime_hash_map_delete_elem_helper);
	return index == BPF_FUNC_map_update_elem &&
	       (fn == reinterpret_cast<void *>(bpftime_map_update_elem_helper) ||
	        fn == reinterpret_cast<void *>(bpftime_array_map_update_elem_helper) ||
	        fn == reinterpret_cast<void *>(bpftime_percpu_array_map_update_elem_helper) ||
	        fn == reinterpret_cast<void *>(bpftime_hash_map_update_elem_helper) ||
	        fn == reinterpret_cast<void *>(bpftime_percpu_hash_map_update_elem_helper));
}
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
	array_map_binding *find_array(uint64_t handle)
	{
		const int native_fd = static_cast<int>(handle);
		if (native_fd < 0)
			return nullptr;
		const uint64_t fd = native_fd;
		const auto &bindings = prog.bound_arrays;
		auto found = bindings.find(fd);
		array_map_binding *binding = nullptr;
		if (found != bindings.end()) {
			binding = found->second.get(); // The executing program owns it.
		} else {
			if (prog.unbound_map_handles.contains(fd))
				return nullptr; // Known non-specialized calls go straight to the original helper.
			// Dynamic handles are not inlined. Keep a temporary reference
			// through the entire invocation, including uses of its result.
			auto &dynamic = dynamic_arrays;
			auto it = dynamic.find(fd);
			if (it == dynamic.end() && fd <= INT_MAX) {
				auto acquired = array_map_binding::acquire(prog.bound_mapping,
					prog.bound_manager, native_fd, std::nullopt);
				if (acquired)
					it = dynamic.emplace(fd, std::move(acquired)).first;
			}
			if (it != dynamic.end())
				binding = it->second.get();
		}
		return binding;
	}
	// Same result as find_array(), but constant FDs of the program resolve
	// through flat tables; known non-specialized maps return null directly.
	array_map_binding *resolve(uint64_t handle)
	{
		const int native_fd = static_cast<int>(handle);
		if (native_fd >= 0 && static_cast<size_t>(native_fd) < prog.bound_by_fd.size()) {
			if (auto *binding = prog.bound_by_fd[native_fd])
				return binding;
			if (prog.unbound_by_fd[native_fd])
				return nullptr;
		}
		return find_array(handle);
	}
	map_helper_fn original(void *cached, unsigned index) const
	{
		return reinterpret_cast<map_helper_fn>(cached ? cached : prog.raw_helpers.at(index));
	}
	static uint64_t lookup(uint64_t fd, uint64_t key, uint64_t a3, uint64_t a4, uint64_t a5)
	{
		if (fd == UINT64_MAX)
			return reinterpret_cast<uint64_t>(detail::array_lookup_enoent());
		auto *scope = active_array_scope;
		if (!scope)
			return reinterpret_cast<uint64_t>(detail::array_lookup_enoent());
		if (auto *binding = scope->resolve(fd)) {
			if (!key)
				return reinterpret_cast<uint64_t>(detail::array_lookup_enoent());
			return reinterpret_cast<uint64_t>(binding->lookup(reinterpret_cast<const void *>(key)));
		}
		// Hash/unknown handles keep the exact original helper and arguments.
		return scope->original(scope->prog.raw_lookup, 1)(fd, key, a3, a4, a5);
	}
	static uint64_t update(uint64_t fd, uint64_t key, uint64_t value, uint64_t flags, uint64_t a5)
	{
		auto *scope = active_array_scope;
		if (!scope) { errno = ENOENT; return uint64_t(-1); }
		if (auto *binding = scope->resolve(fd))
			return static_cast<uint64_t>(binding->update(reinterpret_cast<const void *>(key),
				reinterpret_cast<const void *>(value), flags));
		return scope->original(scope->prog.raw_update, 2)(fd, key, value, flags, a5);
	}
	static uint64_t erase(uint64_t fd, uint64_t key, uint64_t a3, uint64_t a4, uint64_t a5)
	{
		auto *scope = active_array_scope;
		if (!scope) { errno = ENOENT; return uint64_t(-1); }
		if (auto *binding = scope->resolve(fd))
			return static_cast<uint64_t>(binding->erase(reinterpret_cast<const void *>(key)));
		return scope->original(scope->prog.raw_erase, 3)(fd, key, a3, a4, a5);
	}
};

int bpftime_prog::bind_array_maps(bpftime_shm &shm)
{
#if BPFTIME_ENABLE_MPK
	return 0; // Retained mappings have not been audited with MPK transitions.
#endif
#if BPFTIME_ENABLE_MAP_USAGE_STATS
	return 0; // Inlined array accesses would bypass the usage counters.
#endif
	if (is_cuda() || loaded_from_aot || std::string_view(vm_backend_name()) != "llvm" ||
	    get_map_production_ab_mode() != map_production_ab_mode::base)
		return 0;
	auto helper = raw_helpers.find(BPF_FUNC_map_lookup_elem);
	if (helper == raw_helpers.end())
		return 0;
	if (!standard_lookup(helper->second))
		return 0; // Never bypass a custom/diagnostic implementation.
	auto cpu = raw_helpers.find(BPF_FUNC_get_smp_processor_id);
	const bool standard_cpu = cpu != raw_helpers.end() &&
		cpu->second == reinterpret_cast<void *>(bpftime_get_smp_processor_id);
	std::set<int> fds;
	// Global data: LDDW index -> (map FD, offset into element 0).
	std::map<size_t, std::pair<int, uint32_t>> value_refs;
	bool has_lookup = false;
	for (size_t i = 0; i < insns.size(); ++i) {
		const auto &inst = insns[i];
		if (inst.code == 0x18) {
			if (i + 1 == insns.size())
				return 0;
			if (inst.src_reg == BPF_PSEUDO_MAP_FD && inst.imm >= 0)
				fds.insert(inst.imm);
			else if (inst.src_reg == BPF_PSEUDO_MAP_VALUE && inst.imm >= 0)
				value_refs.emplace(i, std::make_pair(inst.imm,
					static_cast<uint32_t>(insns[i + 1].imm)));
			else if (inst.src_reg != 0)
				return 0; // Index, BTF ID, function and other relocations.
			++i;
		} else if (inst.code == 0x8d) {
			return 0; // Unknown indirect helper implementation.
		} else if (inst.code == 0x85) {
			if (inst.src_reg == 1)
				continue; // BPF-to-BPF call within the same program.
			if (inst.src_reg != 0)
				return 0;
			has_lookup |= inst.imm == BPF_FUNC_map_lookup_elem;
		}
	}
	if (!has_lookup || fds.empty())
		return 0;
	auto memory = shm.retain_mapping();
	if (!memory || !shm.get_manager() ||
	    !memory->find<boost::interprocess::interprocess_recursive_mutex>(
		"bpftime_map_binding_mutex_v1").first)
		return 0; // Legacy segments do not advertise synchronized lifetime support.
	map_binding_guard guard(map_binding_mutex(*memory));
	std::map<uint64_t, std::shared_ptr<array_map_binding>> bindings;
	for (int fd : fds) {
		auto binding = array_map_binding::acquire(memory, shm.get_manager(), fd, std::nullopt);
		if (!binding)
			continue; // Hash/non-array maps are not descriptors for this pass.
		if (binding->is_per_cpu() && !standard_cpu)
			continue; // Ordinary arrays can still inline with a custom CPU helper.
		const uint64_t stride = uint64_t(binding->value_size()) * binding->cpu_count();
		if (!binding->value_size() || !binding->max_entries() || !binding->cpu_count() ||
		    stride > UINT32_MAX / binding->max_entries())
			continue; // The existing array implementation uses 32-bit offsets.
		bindings.emplace(fd, std::move(binding));
	}
	if (bindings.empty())
		return 0;
	// A bound program must never resolve global data through a reusable FD:
	// own each map and address it like the kernel's direct value access
	// (single-element ordinary array, offset inside the value). Anything else
	// leaves the whole program on the original path.
	std::map<int, std::shared_ptr<array_map_binding>> values;
	std::map<size_t, uint64_t> value_addresses;
	for (const auto &[index, ref] : value_refs) {
		const auto [fd, offset] = ref;
		auto &binding = values[fd];
		if (!binding) {
			auto shared = bindings.find(fd);
			binding = shared != bindings.end() ? shared->second :
				array_map_binding::acquire(memory, shm.get_manager(), fd, false);
		}
		if (!binding || binding->is_per_cpu() || binding->max_entries() != 1 ||
		    offset >= binding->value_size())
			return 0;
		const uint32_t first = 0;
		auto *base = static_cast<char *>(binding->lookup(&first));
		if (!base)
			return 0;
		value_addresses.emplace(index, reinterpret_cast<uint64_t>(base + offset));
	}
	for (const auto &[fd, binding] : bindings) {
		int result = ebpf_register_array_map_kind(vm, fd,
			reinterpret_cast<uint64_t>(binding->raw_data()), sizeof(uint32_t),
			binding->value_size(), binding->value_size() * binding->cpu_count(), binding->max_entries(),
			binding->is_per_cpu());
		if (result < 0)
			return result; // Abort this program load, never use a partial binding.
	}
	for (int fd : fds)
		if (!bindings.contains(fd))
			unbound_map_handles.insert(fd);
	bound_arrays = std::move(bindings);
	bound_by_fd.assign(static_cast<size_t>(*fds.rbegin()) + 1, nullptr);
	unbound_by_fd.assign(bound_by_fd.size(), 0);
	for (const auto &[fd, binding] : bound_arrays)
		bound_by_fd[fd] = binding.get();
	for (uint64_t fd : unbound_map_handles)
		unbound_by_fd[fd] = 1;
	// Helper slots are fixed from here on (later registrations return EBUSY).
	const std::pair<unsigned, void **> original_helpers[] = {
		{BPF_FUNC_map_lookup_elem, &raw_lookup},
		{BPF_FUNC_map_update_elem, &raw_update},
		{BPF_FUNC_map_delete_elem, &raw_erase}};
	for (const auto &[index, slot] : original_helpers)
		if (auto helper = raw_helpers.find(index); helper != raw_helpers.end())
			*slot = helper->second;
	// Constant handles of non-array maps call the original helpers directly
	// from JIT code; array, dynamic and canonicalized handles keep the
	// dispatcher. Same target as the dispatcher's fallback for these FDs.
	const std::tuple<unsigned, void *, const char *> direct_helpers[] = {
		{BPFTIME_DIRECT_MAP_LOOKUP_SLOT, raw_lookup, BPFTIME_DIRECT_MAP_LOOKUP_NAME},
		{BPFTIME_DIRECT_MAP_UPDATE_SLOT, raw_update, BPFTIME_DIRECT_MAP_UPDATE_NAME},
		{BPFTIME_DIRECT_MAP_DELETE_SLOT, raw_erase, BPFTIME_DIRECT_MAP_DELETE_NAME}};
	if (!unbound_map_handles.empty()) {
		for (const auto &[slot, fn, helper_name] : direct_helpers)
			if (fn)
				if (int result = register_vm_helper(slot, helper_name, fn); result < 0)
					return result;
		for (uint64_t fd : unbound_map_handles)
			if (int result = ebpf_register_direct_map_handle(vm, fd); result < 0)
				return result;
	}
	for (auto &[fd, binding] : values)
		bound_values.push_back(std::move(binding));
	bound_value_addresses = std::move(value_addresses);
	bound_mapping = std::move(memory);
	bound_manager = shm.get_manager();
	if (bound_values.empty())
		SPDLOG_INFO("[array-jit-inline] program={} retained {} maps", name, bound_arrays.size());
	else
		SPDLOG_INFO("[array-jit-inline] program={} retained {} maps and {} global data maps",
			name, bound_arrays.size(), bound_values.size());
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
		jit_only_vm = true;
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
		jit_only_vm = true;
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

int bpftime_prog::register_pending_map_helpers()
{
	if (pending_array_lookup) {
		void *helper = bound_arrays.empty() ? pending_array_lookup :
			reinterpret_cast<void *>(array_jit_execution_scope::lookup);
		const char *helper_name = bound_arrays.empty() ? pending_lookup_name.c_str() :
			"bpftime_bound_array_lookup_v2";
		int result = register_vm_helper(1, helper_name, helper);
		if (result < 0)
			return result;
		pending_array_lookup = nullptr;
	}
	for (const auto &[index, info] : pending_map_mutations) {
		void *helper = info.second;
		if (!bound_arrays.empty())
			helper = index == BPF_FUNC_map_update_elem ?
				reinterpret_cast<void *>(array_jit_execution_scope::update) :
				reinterpret_cast<void *>(array_jit_execution_scope::erase);
		int result = register_vm_helper(index, info.first.c_str(), helper);
		if (result < 0)
			return result;
	}
	pending_map_mutations.clear();
	return 0;
}

namespace
{
// libbpf turns instructions of a branch it knows is dead into calls of helper
// POISON_LDIMM64_MAP_BASE (2001000000) + map index (the load of a map it did
// not create) or POISON_CALL_KFUNC_BASE (2002000000) + extern index (a kfunc it
// did not find), and an instruction whose CO-RE relocation failed (a field
// this kernel lacks, read behind bpf_core_field_exists) into a call of
// 195896080 (0xbad2310, "bad relo"; both halves of an ldimm64); the kernel's
// verifier removes the dead code before they could run. bpftime has no
// verifier, so a program holding them did not compile and was disabled
// (opensnoop: libbpf-tools' compat buffer creates only its ringbuf or its perf
// map; memleak's tracepoint programs: the CO-RE form).
bool is_libbpf_poison_call(const ebpf_inst &inst)
{
	return inst.code == 0x85 && inst.src_reg == 0 &&
	       ((inst.imm >= 2001000000 && inst.imm < 2003000000) ||
		inst.imm == 195896080);
}

uint64_t libbpf_poison_reached(uint64_t, uint64_t, uint64_t, uint64_t,
			       uint64_t)
{
	BPFTIME_LOG_ERROR_RATELIMITED(
		"A program ran an instruction libbpf poisoned (dead code to libbpf, which the kernel would have rejected as reachable)");
	return (uint64_t)(int64_t)-EINVAL;
}

uint64_t dead_call_reached(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)
{
	BPFTIME_LOG_ERROR_RATELIMITED(
		"A program called a helper bpftime does not have, in code its read-only constants made unreachable");
	return (uint64_t)(int64_t)-EINVAL;
}

// A frozen BPF_F_RDONLY_PROG array's value (libbpf's .rodata): constant to
// its programs, as the kernel's verifier treats it.
std::optional<std::span<const uint8_t> > frozen_read_only_value(int fd)
{
	if (bpftime::global_shm_released.load(std::memory_order_acquire))
		return std::nullopt;
	try {
		auto &shm = bpftime::shm_holder.global_shared_memory;
		if (!shm.is_map_fd(fd))
			return std::nullopt;
		const auto &map = std::get<bpftime::bpf_map_handler>(
			shm.get_handler(fd));
		if (!map.is_frozen() || !(map.attr.flags & BPF_F_RDONLY_PROG) ||
		    map.type != bpftime::bpf_map_type::BPF_MAP_TYPE_ARRAY ||
		    map.get_max_entries() < 1)
			return std::nullopt;
		const uint32_t key = 0;
		auto value = (const uint8_t *)map.map_lookup_elem(&key);
		if (value == nullptr)
			return std::nullopt;
		return std::span<const uint8_t>(value, map.get_value_size());
	} catch (...) {
		return std::nullopt;
	}
}
} // namespace

int bpftime_prog::bpftime_prog_load(bool jit)
{
	std::vector<ebpf_inst> owned_insns;
	int res = prepare_load(jit, owned_insns, false);
	if (res < 0)
		return res;
	return finish_load(jit, owned_insns.empty() ? insns : owned_insns);
}

void bpftime_prog::write_bound_value_constants(
	std::vector<ebpf_inst> &load_insns) const
{
	for (const auto &[index, address] : bound_value_addresses) {
		load_insns[index].src_reg = 0;
		load_insns[index].imm = static_cast<int32_t>(address & UINT32_MAX);
		load_insns[index + 1].imm = static_cast<int32_t>(address >> 32);
	}
}

int bpftime_prog::prepare_load(bool jit, std::vector<ebpf_inst> &owned_insns,
			       bool relocatable)
{
	owned_insns.clear();
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
	res = register_pending_map_helpers();
	if (res < 0) {
		array_jit_failed = true;
		return res;
	}

	SPDLOG_DEBUG("Load insn cnt {}", insns.size());
	// Bound global data loads become plain 64-bit constants of the retained
	// address, so no compile (eager or lazy) consults the FD table. The
	// stored program keeps its original relocations. For the AOT cache
	// (relocatable) they stay map value LDDWs, compiled as symbols that
	// bpftime_prog_load_cached defines from the same retained addresses.
	if (!bound_value_addresses.empty() && !relocatable) {
		owned_insns = insns;
		write_bound_value_constants(owned_insns);
	}
	// Poisoned calls go to a helper that reports it ran.
	for (size_t i = 0; i < insns.size(); i++) {
		if (!is_libbpf_poison_call(insns[i]))
			continue;
		if (owned_insns.empty())
			owned_insns = insns;
		owned_insns[i].imm = BPFTIME_LIBBPF_POISON_SLOT;
		if (!libbpf_poison_registered) {
			res = register_vm_helper(BPFTIME_LIBBPF_POISON_SLOT,
					    BPFTIME_LIBBPF_POISON_NAME,
					    (void *)libbpf_poison_reached);
			if (res < 0)
				return res;
			libbpf_poison_registered = true;
		}
	}
	// Calls of helpers nobody registered: if the program's read-only
	// constants make them unreachable (an option that is off), they go to a
	// helper that reports it ran; reachable ones still fail the compile. A
	// kfunc call (BPF_PSEUDO_KFUNC_CALL, imm a BTF id) is never registered.
	auto missing_helper = [&](size_t i) {
		const auto &inst = insns[i];
		if (inst.code == 0x85 && inst.src_reg == 2)
			return true;
		return inst.code == 0x85 && inst.src_reg == 0 &&
		       !is_libbpf_poison_call(inst) && inst.imm >= 0 &&
		       inst.imm < BPFTIME_DEAD_CALL_SLOT &&
		       !raw_helpers.contains(inst.imm) &&
		       !pending_map_mutations.contains(inst.imm);
	};
	bool any_missing = false;
	for (size_t i = 0; i < insns.size() && !any_missing; i++)
		any_missing = missing_helper(i) &&
			      (i == 0 || insns[i - 1].code != 0x18);
	if (any_missing) {
		std::map<int, std::optional<std::span<const uint8_t> > > values;
		auto constant = [&](int fd) {
			auto [itr, fresh] = values.try_emplace(fd);
			if (fresh)
				itr->second = frozen_read_only_value(fd);
			return itr->second;
		};
		const auto reachable = reachable_instructions(insns, constant);
		for (size_t i = 0; i < insns.size(); i++) {
			if (reachable[i] || !missing_helper(i))
				continue;
			if (owned_insns.empty())
				owned_insns = insns;
			owned_insns[i].src_reg = 0;
			owned_insns[i].imm = BPFTIME_DEAD_CALL_SLOT;
			if (!dead_call_registered) {
				res = register_vm_helper(BPFTIME_DEAD_CALL_SLOT,
						    BPFTIME_DEAD_CALL_NAME,
						    (void *)dead_call_reached);
				if (res < 0)
					return res;
				dead_call_registered = true;
			}
		}
	}
	return 0;
}

int bpftime_prog::finish_load(bool jit, const std::vector<ebpf_inst> &load_insns)
{
	int res = ebpf_load(vm, load_insns.data(),
			    load_insns.size() * sizeof(struct ebpf_inst), &errmsg);
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
				jitted = false;
				if (jit_only_vm) {
					// The LLVM VM has no interpreter: its exec
					// would retry the compile on every call.
					jit_failed = true;
					SPDLOG_ERROR("JIT compilation of {} failed ({}); the program is disabled",
						     name, errmsg ? errmsg : "");
				} else {
					SPDLOG_WARN("JIT compilation failed ({}), falling back to interpreter", errmsg);
				}
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
	if (global_shm_released.load(std::memory_order_acquire)) {
		// Process teardown: maps and bindings live in the unmapped shm.
		if (return_val)
			*return_val = 0;
		return -ENODEV;
	}
	if (jit_failed) {
		// Reported once at load; the Frida callback does not log it.
		if (return_val)
			*return_val = 0;
		return -ENOEXEC;
	}
	if (array_jit_requested && (!program_loaded || array_jit_failed)) {
		if (return_val)
			*return_val = 0;
		return -EINVAL;
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
			BPFTIME_LOG_ERROR_RATELIMITED("ebpf_exec returned error: {}", res);
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
	if (!bound_arrays.empty() && (info.index == BPF_FUNC_map_lookup_elem ||
	    info.index == BPF_FUNC_map_update_elem || info.index == BPF_FUNC_map_delete_elem ||
	    info.index == BPF_FUNC_get_smp_processor_id))
		return -EBUSY;
	if (array_jit_requested && info.index == BPF_FUNC_map_lookup_elem &&
	    standard_lookup(info.fn)) {
		if (pending_array_lookup || raw_helpers.contains(info.index))
			return -EEXIST;
		// LLVM helper slots are immutable once registered. Select exactly once
		// at load time, after eligibility and map ownership are established.
		pending_array_lookup = info.fn;
		pending_lookup_name = info.name;
		raw_helpers[info.index] = info.fn;
		signal_ready = false;
		signal_safe_helpers[info.index] = helper_is_async_signal_safe(info.index, info.fn);
		return 0;
	}
	if (pending_array_lookup && info.index == BPF_FUNC_map_lookup_elem)
		return -EEXIST;
	if (pending_map_mutations.contains(info.index))
		return -EEXIST;
	if (array_jit_requested && !program_loaded && standard_mutation(info.index, info.fn)) {
		if (raw_helpers.contains(info.index))
			return -EEXIST;
		pending_map_mutations.emplace(info.index, std::make_pair(info.name, info.fn));
		raw_helpers[info.index] = info.fn;
		signal_ready = false;
		signal_safe_helpers[info.index] = helper_is_async_signal_safe(info.index, info.fn);
		return 0;
	}
	int result = register_vm_helper(info.index, info.name.c_str(), info.fn);
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
	program_loaded = false;
	jitted = false;
	fn = nullptr;
	loaded_from_aot = true;
	// AOT cannot embed retained process-local addresses. Materialize the
	// original helper registrations without attempting any array binding.
	if (int result = register_pending_map_helpers(); result < 0)
		return result;
	// An object compiled after bpftime_prog_load's rewrites (bpftime aot)
	// calls the private slots of poisoned and dead calls.
	if (!libbpf_poison_registered) {
		if (int result = register_vm_helper(BPFTIME_LIBBPF_POISON_SLOT,
					       BPFTIME_LIBBPF_POISON_NAME,
					       (void *)libbpf_poison_reached);
		    result < 0)
			return result;
		libbpf_poison_registered = true;
	}
	if (!dead_call_registered) {
		if (int result = register_vm_helper(BPFTIME_DEAD_CALL_SLOT,
					       BPFTIME_DEAD_CALL_NAME,
					       (void *)dead_call_reached);
		    result < 0)
			return result;
		dead_call_registered = true;
	}
	ebpf_jit_fn res = ebpf_load_aot_object(vm, buf.data(), buf.size());
	if (res == nullptr) {
		SPDLOG_ERROR("Failed to load aot object");
		return -1;
	}
	fn = res;
	jitted = true;
	program_loaded = true;
	return 0;
}

int bpftime_prog::register_vm_helper(unsigned int index,
				     const char *helper_name, void *fn)
{
	int result = ebpf_register(vm, index, helper_name, fn);
	if (result >= 0)
		vm_helper_names[index] = helper_name;
	return result;
}

std::vector<uint8_t>
bpftime_prog::aot_cache_key(const std::vector<ebpf_inst> &load_insns) const
{
	std::vector<uint8_t> key;
	auto put = [&](const void *data, size_t size) {
		auto *bytes = static_cast<const uint8_t *>(data);
		key.insert(key.end(), bytes, bytes + size);
	};
	auto put_u32 = [&](uint32_t value) { put(&value, sizeof(value)); };
	auto put_str = [&](std::string_view text) {
		put_u32(static_cast<uint32_t>(text.size()));
		put(text.data(), text.size());
	};
	put_str("bpftime-aot-cache-v1");
	put_str(vm_backend_name());
	put_u32(static_cast<uint32_t>(get_map_production_ab_mode()));
	put_u32(static_cast<uint32_t>(vm_helper_names.size()));
	for (const auto &[index, helper_name] : vm_helper_names) {
		put_u32(index);
		put_str(helper_name);
	}
	// Inlined arrays (their geometry is in the code), maps whose calls take
	// the original helpers directly, and which map value LDDWs are bound.
	put_u32(static_cast<uint32_t>(bound_arrays.size()));
	for (const auto &[fd, binding] : bound_arrays) {
		put_u32(static_cast<uint32_t>(fd));
		put_u32(binding->value_size());
		put_u32(static_cast<uint32_t>(binding->cpu_count()));
		put_u32(binding->max_entries());
		put_u32(binding->is_per_cpu() ? 1 : 0);
	}
	put_u32(static_cast<uint32_t>(unbound_map_handles.size()));
	for (uint64_t fd : unbound_map_handles)
		put_u32(static_cast<uint32_t>(fd));
	put_u32(static_cast<uint32_t>(bound_values.size()));
	put_u32(static_cast<uint32_t>(bound_value_addresses.size()));
	for (const auto &entry : bound_value_addresses)
		put_u32(static_cast<uint32_t>(entry.first));
	put_u32(static_cast<uint32_t>(load_insns.size()));
	put(load_insns.data(), load_insns.size() * sizeof(ebpf_inst));
	return key;
}

namespace
{
// Whether the AOT cache can hold the program's code: every address of the
// loading process in it must be a symbol. Map values (LDDW pseudo map value)
// are, as are the value bases of inlined arrays; map indexes, BTF ids and
// functions in LDDWs are not.
bool aot_cacheable(const std::vector<ebpf_inst> &load_insns)
{
	for (size_t i = 0; i < load_insns.size(); i++) {
		if (load_insns[i].code != 0x18)
			continue;
		if (load_insns[i].src_reg != 0 &&
		    load_insns[i].src_reg != BPF_PSEUDO_MAP_FD &&
		    load_insns[i].src_reg != BPF_PSEUDO_MAP_VALUE)
			return false;
		i++; // The second half of the LDDW.
	}
	return true;
}

// llvmbpf's symbol of a pseudo map value (compiler.cpp,
// kernel_pseudo_map_value_symbol) and of an inlined array's value base
// (llvm_jit_context.cpp, bpftime_array_value_base_symbol).
std::string map_value_symbol(int32_t fd, int32_t offset)
{
	char name[64];
	snprintf(name, sizeof(name), "__llvmbpf_pseudo_map_value_fd_%08x_off_%08x",
		 static_cast<uint32_t>(fd), static_cast<uint32_t>(offset));
	return name;
}
std::string array_value_base_symbol(uint64_t fd)
{
	return "__bpftime_array_value_base_" + std::to_string(fd);
}
} // namespace

int bpftime_prog::compile_aot_cache_object(std::vector<uint8_t> &object,
					   std::vector<uint8_t> &key)
{
	object.clear();
	key.clear();
	if (is_cuda() || std::string_view(vm_backend_name()) != "llvm")
		return -ENOTSUP;
	std::vector<ebpf_inst> owned_insns;
	int res = prepare_load(true, owned_insns, true);
	if (res < 0)
		return res;
	const auto &load_insns = owned_insns.empty() ? insns : owned_insns;
	if (!aot_cacheable(load_insns))
		return -ENOTSUP;
	res = ebpf_set_relocatable_map_addresses(vm, true);
	if (res < 0)
		return res;
	res = ebpf_load(vm, load_insns.data(),
			load_insns.size() * sizeof(struct ebpf_inst), &errmsg);
	if (res < 0)
		return res;
	auto compiled = vm->vm_instance->do_aot_compile(false);
	if (compiled.empty())
		return -EINVAL;
	object = std::move(compiled);
	key = aot_cache_key(load_insns);
	return 0;
}

int bpftime_prog::bpftime_prog_load_cached(const uint8_t *object,
					   size_t object_size,
					   const uint8_t *key, size_t key_size)
{
	std::vector<ebpf_inst> owned_insns;
	int res = prepare_load(true, owned_insns, true);
	if (res < 0)
		return res;
	if (owned_insns.empty())
		owned_insns = insns;
	// Compiling instead: bound global data as constants, as
	// bpftime_prog_load does.
	auto compile = [&] {
		write_bound_value_constants(owned_insns);
		return finish_load(true, owned_insns);
	};
	const auto expected = aot_cache_key(owned_insns);
	if (expected.size() != key_size ||
	    memcmp(expected.data(), key, key_size) != 0) {
		SPDLOG_INFO(
			"Program {} differs from what its AOT object was compiled from; compiling it",
			name);
		return compile();
	}
	// This process's addresses: bound global data at the retained
	// bindings, other map values where JIT code would have them (map_val
	// at compile time), inlined arrays at their retained storage.
	for (size_t i = 0; i + 1 < owned_insns.size(); i++) {
		const auto &inst = owned_insns[i];
		if (inst.code != 0x18)
			continue;
		if (inst.src_reg == BPF_PSEUDO_MAP_VALUE) {
			const int32_t offset = owned_insns[i + 1].imm;
			auto bound = bound_value_addresses.find(i);
			const uint64_t address =
				bound != bound_value_addresses.end() ?
					bound->second :
					map_val(map_ptr_by_fd(inst.imm)) +
						offset; // As compiler.cpp.
			res = ebpf_define_aot_symbol(
				vm, map_value_symbol(inst.imm, offset).c_str(),
				address);
			if (res < 0)
				return compile();
		}
		i++;
	}
	for (const auto &[fd, binding] : bound_arrays) {
		res = ebpf_define_aot_symbol(
			vm, array_value_base_symbol(fd).c_str(),
			reinterpret_cast<uint64_t>(binding->raw_data()));
		if (res < 0)
			return compile();
	}
	ebpf_jit_fn loaded = nullptr;
	try {
		loaded = ebpf_load_aot_object(vm, object, object_size);
	} catch (const std::exception &ex) {
		SPDLOG_WARN("Linking the AOT object of {} threw: {}", name,
			    ex.what());
	}
	if (loaded == nullptr) {
		SPDLOG_WARN("Linking the AOT object of {} failed; compiling it",
			    name);
		return compile();
	}
	fn = loaded;
	jitted = true;
	loaded_from_aot = true;
	program_loaded = true;
	return 0;
}

} // namespace bpftime

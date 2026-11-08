/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#ifndef _BPFTIME_PROG_HPP
#define _BPFTIME_PROG_HPP

#include "bpftime_config.hpp"
#include <ebpf-vm.h>
#include <optional>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <memory>
#include <cerrno>
#include <boost/interprocess/managed_shared_memory.hpp>
namespace bpftime
{
class bpftime_shm;
class handler_manager;
class array_map_binding;
struct array_jit_execution_scope;

extern thread_local std::optional<uint64_t> current_thread_bpf_cookie;
extern thread_local std::optional<uint64_t> current_thread_tail_call_ret;
#ifdef BPFTIME_ENABLE_CUDA_ATTACH
std::optional<std::vector<char>> compile_ptx_to_elf(const std::string &ptx_code,
						    const char *cpu_target);
#endif
// executable program for bpf function
class bpftime_prog {
    public:
	const char *prog_name() const
	{
		return name.c_str();
	}
	bpftime_prog(const ebpf_inst *insn, size_t insn_cnt, const char *name);
	bpftime_prog(const ebpf_inst *insn, size_t insn_cnt, const char *name,
		     runtime_config config);
	~bpftime_prog();

	// load the programs to userspace vm or compile the jit program
	// if program_name is NULL, will load the first program in the object
	int bpftime_prog_load(bool jit);
	int bpftime_prog_unload();
	// Validate all external calls and compile before installing a signal probe.
	// Returns -ENOTSUP for unaudited helpers or an unavailable eager JIT.
	int prepare_for_signal_execution();

	// exec in user space
	int bpftime_prog_exec(void *memory, size_t memory_size,
			      uint64_t *return_val) const;

	int bpftime_prog_register_raw_helper(struct bpftime_helper_info info);
	// Compatibility entry point; eligible LLVM JIT lookups are now automatic.
	int request_array_map_inlining()
	{
		if (program_loaded || raw_helpers.contains(1))
			return -EBUSY;
		array_jit_requested = true;
		return 0;
	}
	const std::vector<ebpf_inst> &get_insns() const
	{
		return insns;
	}
	const struct ebpf_vm *get_vm() const
	{
		return vm;
	}
	const char *vm_backend_name() const { return ebpf_get_vm_name(vm); }
	int load_aot_object(const std::vector<uint8_t> &buf);
	// The AOT cache (bpftime_prog_cache.cpp). compile_aot_cache_object
	// prepares the program as bpftime_prog_load(true) does and compiles an
	// AOT object instead of JIT code, with the key it was compiled from;
	// -ENOTSUP for programs it does not cache. bpftime_prog_load_cached
	// prepares the program the same way and links object when the key it
	// computes equals key; otherwise it compiles as bpftime_prog_load(true).
	int compile_aot_cache_object(std::vector<uint8_t> &object,
				     std::vector<uint8_t> &key);
	int bpftime_prog_load_cached(const uint8_t *object, size_t object_size,
				     const uint8_t *key, size_t key_size);
	bool is_loaded_from_aot() const
	{
		return loaded_from_aot;
	}
#ifdef BPFTIME_ENABLE_CUDA_ATTACH
	bool is_cuda() const
	{
		return name.starts_with("cuda__");
	}
	const void *get_cuda_elf_binary() const
	{
		return cuda_elf_binary->data();
	}

	const std::string &get_ptx_code() const
	{
		return *ptx_code;
	}
#else
	bool is_cuda() const
	{
		return false;
	}

#endif
    private:
	friend struct array_jit_execution_scope;
	int bind_array_maps(bpftime_shm &shm);
	bool array_jit_requested = true;
	bool array_jit_failed = false;
	std::map<uint64_t, std::shared_ptr<array_map_binding>> bound_arrays;
	std::set<uint64_t> unbound_map_handles;
	// Direct map-value (global data) maps of a bound program, and the owned
	// address each such LDDW is rewritten to (keyed by instruction index).
	std::vector<std::shared_ptr<array_map_binding>> bound_values;
	std::map<size_t, uint64_t> bound_value_addresses;
	// O(1) dispatch of a bound program's non-inlined map calls, indexed by the
	// constant FDs it references: owning binding (or null) and whether the FD
	// is a known non-specialized map. Other handles take the dynamic path.
	std::vector<array_map_binding *> bound_by_fd;
	std::vector<uint8_t> unbound_by_fd;
	void *raw_lookup = nullptr;
	void *raw_update = nullptr;
	void *raw_erase = nullptr;
	std::shared_ptr<boost::interprocess::managed_shared_memory> bound_mapping;
	const handler_manager *bound_manager = nullptr;
	std::map<unsigned, void *> raw_helpers;
	void *pending_array_lookup = nullptr;
	std::string pending_lookup_name;
	std::map<unsigned, std::pair<std::string, void *>> pending_map_mutations;
	int register_pending_map_helpers();
	int bpftime_prog_set_insn(struct ebpf_inst *insn, size_t insn_cnt);
	// bpftime_prog_load in two parts: binding, helper registration and the
	// instruction rewrites (owned_insns, empty when insns are loaded as
	// they are), then loading and compiling load_insns. relocatable (the
	// AOT cache) keeps bound global data LDDWs instead of rewriting them.
	int prepare_load(bool jit, std::vector<ebpf_inst> &owned_insns,
			 bool relocatable);
	// Bound global data LDDWs as constants of their retained addresses.
	void write_bound_value_constants(std::vector<ebpf_inst> &load_insns) const;
	int finish_load(bool jit, const std::vector<ebpf_inst> &load_insns);
	// What the compiler sees for load_insns: the VM, the helpers registered
	// with it by index and name, the bindings and the instructions.
	std::vector<uint8_t>
	aot_cache_key(const std::vector<ebpf_inst> &load_insns) const;
	// ebpf_register, recording the name for aot_cache_key.
	int register_vm_helper(unsigned int index, const char *helper_name,
			       void *fn);
	std::map<unsigned int, std::string> vm_helper_names;
	std::string name;
	// vm at the first element
	struct ebpf_vm *vm;

	bool jitted = false;
	// The VM can only run JIT code (llvm), and the JIT compile failed: the
	// program is disabled instead of recompiling on every invocation.
	bool jit_only_vm = false;
	bool jit_failed = false;
	bool libbpf_poison_registered = false;
	bool dead_call_registered = false;
	bool program_loaded = false;
	bool signal_ready = false;
	bool loaded_from_aot = false;
	std::map<unsigned, bool> signal_safe_helpers;

	// used in jit
	ebpf_jit_fn fn = nullptr;
	std::vector<struct ebpf_inst> insns;

	char *errmsg = nullptr;

	// ufunc ctx
	struct bpftime_ufunc_ctx *ufunc_ctx;

#ifdef BPFTIME_ENABLE_CUDA_ATTACH
	// ptx code
	std::optional<std::string> ptx_code;
	// cuda binary
	std::optional<std::vector<char>> cuda_elf_binary;
#endif
};

} // namespace bpftime
#endif

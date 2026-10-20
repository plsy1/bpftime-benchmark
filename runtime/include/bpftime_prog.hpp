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
	// Requested by the runtime loader, never enabled globally or for AOT.
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
	bool array_jit_requested = false;
	bool array_jit_failed = false;
	std::map<uint64_t, std::shared_ptr<array_map_binding>> bound_arrays;
	std::shared_ptr<boost::interprocess::managed_shared_memory> bound_mapping;
	const handler_manager *bound_manager = nullptr;
	std::map<unsigned, void *> raw_helpers;
	void *pending_array_lookup = nullptr;
	int bpftime_prog_set_insn(struct ebpf_inst *insn, size_t insn_cnt);
	std::string name;
	// vm at the first element
	struct ebpf_vm *vm;

	bool jitted = false;
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

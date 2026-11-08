/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026, eunomia-bpf org
 * All rights reserved.
 */
#ifndef _BPFTIME_PROG_CACHE_HPP
#define _BPFTIME_PROG_CACHE_HPP

#include "bpftime_config.hpp"
#include "ebpf-vm.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace bpftime
{
class handler_manager;

// False when BPFTIME_AOT_CACHE is "0": the server compiles no AOT objects
// and agents ignore the ones they find.
bool aot_cache_enabled();

// Registers the helpers an agent would give the program (with the uprobe
// attach impls' custom helpers) and compiles its AOT object and key
// (bpftime_prog::compile_aot_cache_object). Returns a negative errno and
// leaves both empty when the program is not cached.
int compile_aot_cache_entry(const handler_manager *manager,
			    const ebpf_inst *insns, size_t insn_cnt,
			    const char *name, const runtime_config &config,
			    std::vector<uint8_t> &object,
			    std::vector<uint8_t> &key);
} // namespace bpftime

// bpftime_progs_create, with the program's AOT object compiled and stored
// with it when the cache is enabled (the syscall server's BPF_PROG_LOAD).
int bpftime_progs_create_cached(int fd, const ebpf_inst *insn,
				size_t insn_cnt, const char *prog_name,
				int prog_type);

#endif

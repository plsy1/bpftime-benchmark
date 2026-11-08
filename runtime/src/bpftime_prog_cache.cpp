/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026, eunomia-bpf org
 * All rights reserved.
 */
// The AOT cache: the syscall server compiles each program once, when it is
// loaded, into an AOT object kept with the program in shared memory, and
// agents link that object instead of compiling the program in every traced
// process (bpf_attach_ctx::instantiate_prog_handler_at). An agent uses the
// object only if the key it computes for the program (bpftime_prog::
// aot_cache_key) equals the key stored with it.
#include "bpftime_prog_cache.hpp"
#include "bpf_attach_ctx.hpp"
#include "bpftime_prog.hpp"
#include "bpftime_shm.hpp"
#include "uprobe_custom_helpers.hpp"
#include <bpftime_shm_internal.hpp>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <spdlog/spdlog.h>

namespace bpftime
{

bool aot_cache_enabled()
{
	// Read at every program load (a getenv), so tests can switch it.
	const char *value = getenv("BPFTIME_AOT_CACHE");
	return value == nullptr || strcmp(value, "0") != 0;
}

namespace
{
// Never called: the server only compiles. The agent's own functions are
// linked into the object by index.
uint64_t uprobe_helper_placeholder(uint64_t, uint64_t, uint64_t, uint64_t,
				   uint64_t)
{
	return 0;
}
} // namespace

int compile_aot_cache_entry(const handler_manager *manager,
			    const ebpf_inst *insns, size_t insn_cnt,
			    const char *name, const runtime_config &config,
			    std::vector<uint8_t> &object,
			    std::vector<uint8_t> &key)
{
	object.clear();
	key.clear();
	if (!config.jit_enabled || manager == nullptr)
		return -ENOTSUP;
	// The custom helpers agents get from their uprobe attach impl.
	std::map<int, bpftime_helper_info> custom;
	for (const auto &helper : attach::uprobe_custom_helpers)
		custom[helper.index] = bpftime_helper_info{
			.index = static_cast<unsigned int>(helper.index),
			.name = helper.name,
			.fn = reinterpret_cast<void *>(
				uprobe_helper_placeholder),
		};
	bpftime_prog prog(insns, insn_cnt, name);
	if (int err = register_prog_handler_helpers(manager, insns, insn_cnt,
						    &prog, config, &custom);
	    err < 0)
		return err;
	return prog.compile_aot_cache_object(object, key);
}

} // namespace bpftime

int bpftime_progs_create_cached(int fd, const ebpf_inst *insn,
				size_t insn_cnt, const char *prog_name,
				int prog_type)
{
	using namespace bpftime;
	std::vector<uint8_t> object, key;
	if (aot_cache_enabled()) {
		try {
			int err = compile_aot_cache_entry(
				shm_holder.global_shared_memory.get_manager(),
				insn, insn_cnt, prog_name,
				bpftime_get_runtime_config(), object, key);
			if (err < 0) {
				SPDLOG_DEBUG(
					"No AOT object for program {} ({}); agents compile it",
					prog_name, err);
				object.clear();
				key.clear();
			}
		} catch (const std::exception &ex) {
			SPDLOG_WARN(
				"Compiling the AOT object of program {} failed: {}; agents compile it",
				prog_name, ex.what());
			object.clear();
			key.clear();
		}
	}
	return shm_holder.global_shared_memory.add_bpf_prog(
		fd, insn, insn_cnt, prog_name, prog_type, object, key);
}

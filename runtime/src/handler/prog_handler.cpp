/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#include <handler/prog_handler.hpp>
namespace bpftime
{
bpf_prog_handler::bpf_prog_handler(managed_shared_memory &mem,
				   const struct ebpf_inst *insn,
				   size_t insn_cnt, const char *prog_name,
				   int prog_type)
	: insns(shm_ebpf_inst_vector_allocator(mem.get_segment_manager())),
	aot_insns(shm_aot_inst_vector_allocator(mem.get_segment_manager())),
	  aot_key(shm_aot_inst_vector_allocator(mem.get_segment_manager())),
	  name(char_allocator(mem.get_segment_manager()))
{
	insns.assign(insn, insn + insn_cnt);
	this->name = prog_name;
}

bpf_prog_handler::bpf_prog_handler(managed_shared_memory &mem,
				   const struct ebpf_inst *insn,
				   size_t insn_cnt, const char *prog_name,
				   int prog_type,
				   const std::vector<uint8_t> &aot_object,
				   const std::vector<uint8_t> &aot_object_key)
	: bpf_prog_handler(mem, insn, insn_cnt, prog_name, prog_type)
{
	// A key without its object, or the reverse, is no cache entry.
	if (aot_object.empty() || aot_object_key.empty())
		return;
	aot_insns.assign(aot_object.begin(), aot_object.end());
	aot_key.assign(aot_object_key.begin(), aot_object_key.end());
}
} // namespace bpftime

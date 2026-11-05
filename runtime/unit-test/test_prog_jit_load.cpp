#include "bpftime_config.hpp"
#include "bpftime_prog.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cerrno>
#include <cstdint>

using namespace bpftime;

// libbpf-tools funclatency's kretprobe starts with a bpf-to-bpf call (its
// BPF_KRETPROBE body was not inlined). The JIT never registered the return
// block of a call at pc 0 and crashed the traced process while compiling.
TEST_CASE("JIT compiles a bpf-to-bpf call at pc 0", "[jit_load]")
{
	const ebpf_inst code[] = {
		// call the subprogram at pc 3
		{ .code = 0x85, .dst_reg = 0, .src_reg = 1, .off = 0, .imm = 2 },
		// r0 += 1
		{ .code = 0x07, .dst_reg = 0, .src_reg = 0, .off = 0, .imm = 1 },
		{ .code = 0x95, .dst_reg = 0, .src_reg = 0, .off = 0, .imm = 0 },
		// subprogram: r0 = 41
		{ .code = 0xb7, .dst_reg = 0, .src_reg = 0, .off = 0, .imm = 41 },
		{ .code = 0x95, .dst_reg = 0, .src_reg = 0, .off = 0, .imm = 0 },
	};
	bpftime_prog prog(code, 5, "call_at_pc0", runtime_config{});
	REQUIRE(prog.bpftime_prog_load(true) == 0);
	uint64_t memory[4] = {};
	uint64_t ret = 0;
	REQUIRE(prog.bpftime_prog_exec(memory, sizeof(memory), &ret) == 0);
	REQUIRE(ret == 42);
}

// A program calling an unregistered helper (funclatency's helper 37) cannot
// be JIT compiled. The LLVM VM has no interpreter and retried the compile on
// every invocation (about 1 ms each, logged each time); the program is now
// disabled once.
TEST_CASE("A program whose JIT compile fails is disabled", "[jit_load]")
{
	const ebpf_inst code[] = {
		{ .code = 0x85, .dst_reg = 0, .src_reg = 0, .off = 0, .imm = 37 },
		{ .code = 0x95, .dst_reg = 0, .src_reg = 0, .off = 0, .imm = 0 },
	};
	bpftime_prog prog(code, 2, "missing_helper", runtime_config{});
	REQUIRE(prog.bpftime_prog_load(true) == 0);
	uint64_t memory[4] = {};
	for (int i = 0; i < 3; i++) {
		uint64_t ret = 7;
		REQUIRE(prog.bpftime_prog_exec(memory, sizeof(memory), &ret) ==
			-ENOEXEC);
		REQUIRE(ret == 0);
	}
}

// libbpf turns a dead branch's map load (a map it did not create) into two
// calls of helper 2001000000 + map index; the kernel's verifier drops the dead
// code. Such a program failed to compile in bpftime and was disabled
// (opensnoop's compat buffer, problem 11 of uprobe/harness-gaps-20261104).
TEST_CASE("Calls libbpf poisoned in dead code do not disable the program",
	  "[jit_load]")
{
	const ebpf_inst dead[] = {
		// r0 = 7; if r0 != 0 goto exit
		{ .code = 0xb7, .dst_reg = 0, .src_reg = 0, .off = 0, .imm = 7 },
		{ .code = 0x55, .dst_reg = 0, .src_reg = 0, .off = 2, .imm = 0 },
		// the poisoned ldimm64 of map 3
		{ .code = 0x85, .dst_reg = 0, .src_reg = 0, .off = 0,
		  .imm = 2001000003 },
		{ .code = 0x85, .dst_reg = 0, .src_reg = 0, .off = 0,
		  .imm = 2001000003 },
		{ .code = 0x95, .dst_reg = 0, .src_reg = 0, .off = 0, .imm = 0 },
	};
	bpftime_prog prog(dead, 5, "poison_dead", runtime_config{});
	REQUIRE(prog.bpftime_prog_load(true) == 0);
	uint64_t memory[4] = {};
	uint64_t ret = 0;
	REQUIRE(prog.bpftime_prog_exec(memory, sizeof(memory), &ret) == 0);
	REQUIRE(ret == 7);

	// Reached (the kernel would have refused the program): the call
	// returns -EINVAL and reports it.
	const ebpf_inst reached[] = {
		{ .code = 0x85, .dst_reg = 0, .src_reg = 0, .off = 0,
		  .imm = 2002000001 },
		{ .code = 0x95, .dst_reg = 0, .src_reg = 0, .off = 0, .imm = 0 },
	};
	bpftime_prog reached_prog(reached, 2, "poison_reached",
				  runtime_config{});
	REQUIRE(reached_prog.bpftime_prog_load(true) == 0);
	REQUIRE(reached_prog.bpftime_prog_exec(memory, sizeof(memory), &ret) ==
		0);
	REQUIRE((int64_t)ret == -EINVAL);
}

// The CO-RE form: an instruction whose relocation failed (a field the running
// kernel lacks, read behind bpf_core_field_exists) becomes a call of 195896080
// (0xbad2310). memleak's tracepoint programs were disabled for it (review
// 2026-11-05).
TEST_CASE("Calls libbpf poisoned for a failed CO-RE relocation do not disable the program",
	  "[jit_load]")
{
	const ebpf_inst dead[] = {
		{ .code = 0xb7, .dst_reg = 0, .src_reg = 0, .off = 0, .imm = 7 },
		{ .code = 0x55, .dst_reg = 0, .src_reg = 0, .off = 1, .imm = 0 },
		{ .code = 0x85, .dst_reg = 0, .src_reg = 0, .off = 0,
		  .imm = 195896080 },
		{ .code = 0x95, .dst_reg = 0, .src_reg = 0, .off = 0, .imm = 0 },
	};
	bpftime_prog prog(dead, 4, "core_poison_dead", runtime_config{});
	REQUIRE(prog.bpftime_prog_load(true) == 0);
	uint64_t memory[4] = {};
	uint64_t ret = 0;
	REQUIRE(prog.bpftime_prog_exec(memory, sizeof(memory), &ret) == 0);
	REQUIRE(ret == 7);

	const ebpf_inst reached[] = {
		{ .code = 0x85, .dst_reg = 0, .src_reg = 0, .off = 0,
		  .imm = 195896080 },
		{ .code = 0x95, .dst_reg = 0, .src_reg = 0, .off = 0, .imm = 0 },
	};
	bpftime_prog reached_prog(reached, 2, "core_poison_reached",
				  runtime_config{});
	REQUIRE(reached_prog.bpftime_prog_load(true) == 0);
	REQUIRE(reached_prog.bpftime_prog_exec(memory, sizeof(memory), &ret) ==
		0);
	REQUIRE((int64_t)ret == -EINVAL);
}

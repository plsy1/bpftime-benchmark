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

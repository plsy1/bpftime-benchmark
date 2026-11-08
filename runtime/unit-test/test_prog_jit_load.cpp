#include "bpftime_config.hpp"
#include "bpftime_prog.hpp"
#include "bpftime_vm_compat.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cerrno>
#include <cstdint>
#include <vector>

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

// A bpf-to-bpf call moved r10 by 64 bytes instead of one 512-byte frame, so
// the callee's stack overlapped the caller's below r10-64 and overwrote it
// (results gpu/examples-20261109, problem 4). Each level here fills its whole
// frame, calls the next level, then returns the sum of its own frame plus the
// callee's result: any overlap changes the sum.
static std::vector<ebpf_inst> nested_frame_program(int levels)
{
	std::vector<ebpf_inst> code;
	auto emit = [&](uint8_t op, uint8_t dst, uint8_t src, int16_t off,
			int32_t imm) {
		code.push_back(ebpf_inst{ .code = op,
					  .dst_reg = dst,
					  .src_reg = src,
					  .off = off,
					  .imm = imm });
	};
	for (int level = 0; level < levels; level++) {
		const bool last = level == levels - 1;
		// *(u64 *)(r10 - 8k) = level << 16 | k
		for (int k = 1; k <= 64; k++)
			emit(0x7a, 10, 0, (int16_t)(-8 * k), (level << 16) | k);
		size_t call = 0;
		if (!last) {
			call = code.size();
			emit(0x85, 0, 1, 0, 0); // call the next level (below)
			emit(0xbf, 6, 0, 0, 0); // r6 = r0
		}
		emit(0xb7, 0, 0, 0, 0); // r0 = 0
		for (int k = 1; k <= 64; k++) {
			emit(0x79, 1, 10, (int16_t)(-8 * k), 0); // r1 = *(r10 - 8k)
			emit(0x0f, 0, 1, 0, 0); // r0 += r1
		}
		if (!last)
			emit(0x0f, 0, 6, 0, 0); // r0 += r6
		emit(0x95, 0, 0, 0, 0);
		if (!last)
			code[call].imm = (int32_t)(code.size() - (call + 1));
	}
	return code;
}

static uint64_t nested_frame_sum(int levels)
{
	uint64_t sum = 0;
	for (int level = 0; level < levels; level++)
		for (int k = 1; k <= 64; k++)
			sum += (uint64_t)((level << 16) | k);
	return sum;
}

TEST_CASE("A bpf-to-bpf callee's stack frame does not overlap its caller's",
	  "[jit_load]")
{
	// 8: the kernel's limit of call frames (MAX_CALL_FRAMES).
	for (int levels : { 2, 8 }) {
		CAPTURE(levels);
		auto code = nested_frame_program(levels);
		bpftime_prog prog(code.data(), code.size(), "nested_frames",
				  runtime_config{});
		REQUIRE(prog.bpftime_prog_load(true) == 0);
		uint64_t memory[4] = {};
		uint64_t ret = 0;
		REQUIRE(prog.bpftime_prog_exec(memory, sizeof(memory), &ret) ==
			0);
		REQUIRE(ret == nested_frame_sum(levels));

		// The agent loads the objects the syscall server compiled (the
		// AOT cache): the same frames there.
		REQUIRE(prog.get_vm() != nullptr);
		REQUIRE(prog.get_vm()->vm_instance != nullptr);
		auto object = prog.get_vm()->vm_instance->do_aot_compile(false);
		REQUIRE(!object.empty());
		bpftime_prog loaded(code.data(), code.size(),
				    "nested_frames_aot", runtime_config{});
		REQUIRE(loaded.load_aot_object(object) == 0);
		ret = 0;
		REQUIRE(loaded.bpftime_prog_exec(memory, sizeof(memory),
						 &ret) == 0);
		REQUIRE(ret == nested_frame_sum(levels));
	}
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

// `bpftime aot` compiles after bpftime_prog_load's rewrites: the object calls
// the private slots, which load_aot_object did not register (review
// 2026-11-05).
TEST_CASE("An AOT object of a program with poisoned calls loads", "[jit_load]")
{
	// r0 = 7; if *(u64 *)r1 != 0 skip the call: a branch on the input, so
	// that LLVM keeps the call (a constant one it folds away).
	const ebpf_inst code[] = {
		{ .code = 0xb7, .dst_reg = 0, .src_reg = 0, .off = 0, .imm = 7 },
		{ .code = 0x79, .dst_reg = 2, .src_reg = 1, .off = 0, .imm = 0 },
		{ .code = 0x55, .dst_reg = 2, .src_reg = 0, .off = 1, .imm = 0 },
		{ .code = 0x85, .dst_reg = 0, .src_reg = 0, .off = 0,
		  .imm = 195896080 },
		{ .code = 0x95, .dst_reg = 0, .src_reg = 0, .off = 0, .imm = 0 },
	};
	bpftime_prog compiled(code, 5, "aot_poison", runtime_config{});
	REQUIRE(compiled.bpftime_prog_load(true) == 0);
	REQUIRE(compiled.get_vm() != nullptr);
	REQUIRE(compiled.get_vm()->vm_instance != nullptr);
	auto object = compiled.get_vm()->vm_instance->do_aot_compile(false);
	REQUIRE(!object.empty());
	bpftime_prog loaded(code, 5, "aot_poison_loaded", runtime_config{});
	REQUIRE(loaded.load_aot_object(object) == 0);
	uint64_t memory[4] = { 1 };
	uint64_t ret = 0;
	REQUIRE(loaded.bpftime_prog_exec(memory, sizeof(memory), &ret) == 0);
	REQUIRE(ret == 7);
	memory[0] = 0; // reached: reported, -EINVAL
	REQUIRE(loaded.bpftime_prog_exec(memory, sizeof(memory), &ret) == 0);
	REQUIRE((int64_t)ret == -EINVAL);
}

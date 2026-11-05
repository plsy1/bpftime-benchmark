#include "bpftime_prog_reachability.hpp"
#include "bpftime_prog.hpp"
#include "bpftime_shm.hpp"
#include "common_def.hpp"
#include <boost/scope_exit.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <linux/bpf.h>
#include <span>
#include <string>
#include <unistd.h>
#include <vector>

using namespace bpftime;

namespace
{
constexpr int RODATA_FD = 5;

ebpf_inst insn(uint8_t code, uint8_t dst, uint8_t src, int16_t off,
	       int32_t imm)
{
	ebpf_inst i{};
	i.code = code;
	i.dst_reg = dst;
	i.src_reg = src;
	i.off = off;
	i.imm = imm;
	return i;
}

// r1 = &rodata + 0 (ldimm64 map_value); r1 = *(u8 *)(r1 + 0);
// if r1 == 0 goto +1; call 158; r0 = 7; exit
std::vector<ebpf_inst> option_guarded_call(uint8_t jump = 0x15)
{
	return {
		insn(0x18, 1, 2, 0, RODATA_FD), insn(0, 0, 0, 0, 0),
		insn(0x71, 1, 1, 0, 0),		insn(jump, 1, 0, 1, 0),
		insn(0x85, 0, 0, 0, 158),	insn(0xb7, 0, 0, 0, 7),
		insn(0x95, 0, 0, 0, 0),
	};
}

constant_map_value rodata(const std::vector<uint8_t> &bytes)
{
	return [&bytes](int fd) -> std::optional<std::span<const uint8_t> > {
		if (fd != RODATA_FD)
			return std::nullopt;
		return std::span<const uint8_t>(bytes);
	};
}
} // namespace

TEST_CASE("Branches on read-only constants are followed one way",
	  "[jit_load][reachability]")
{
	const std::vector<uint8_t> off{ 0 }, on{ 1 };
	auto code = option_guarded_call();
	// The option off: the call cannot run; everything else can.
	auto reachable = reachable_instructions(code, rodata(off));
	CHECK_FALSE(reachable[4]);
	CHECK(reachable[0]);
	CHECK(reachable[1]);
	CHECK(reachable[5]);
	CHECK(reachable[6]);
	// The option on, or a map that is not constant: the call can run.
	CHECK(reachable_instructions(code, rodata(on))[4]);
	CHECK(reachable_instructions(code, [](int) {
		      return std::optional<std::span<const uint8_t> >();
	      })[4]);
	// jne decided the other way, JMP32 the same.
	CHECK(reachable_instructions(option_guarded_call(0x55), rodata(off))[4]);
	CHECK_FALSE(
		reachable_instructions(option_guarded_call(0x16), rodata(off))[4]);
	// A load past the value is not a constant.
	auto past = option_guarded_call();
	past[2].off = 1;
	CHECK(reachable_instructions(past, rodata(off))[4]);
}

TEST_CASE("Reachability keeps subprograms, callbacks and joined paths",
	  "[jit_load][reachability]")
{
	const std::vector<uint8_t> off{ 0 };
	// A bpf-to-bpf call's target and a callback (ldimm64 src 4) can run,
	// whatever the constants.
	std::vector<ebpf_inst> calls{
		insn(0x85, 0, 1, 0, 3),	    // call pc 4
		insn(0x18, 2, 4, 0, 4),	    // r2 = &callback (pc 1 + 4 + 1)
		insn(0, 0, 0, 0, 0),	    insn(0x95, 0, 0, 0, 0),
		insn(0xb7, 0, 0, 0, 1),	    // pc 4: subprogram
		insn(0x95, 0, 0, 0, 0),	    insn(0x85, 0, 0, 0, 158), // pc 6
		insn(0x95, 0, 0, 0, 0),
	};
	auto reachable = reachable_instructions(calls, rodata(off));
	CHECK(reachable[4]);
	CHECK(reachable[6]);
	// The constant overwritten on one path: unknown after the join.
	std::vector<ebpf_inst> joined{
		insn(0x18, 1, 2, 0, RODATA_FD), insn(0, 0, 0, 0, 0),
		insn(0x71, 1, 1, 0, 0),
		insn(0x15, 2, 0, 1, 0),	   // if r2 == 0 (unknown) goto +1
		insn(0xb7, 1, 0, 0, 1),	   // r1 = 1
		insn(0x15, 1, 0, 1, 0),	   // if r1 == 0 goto +1
		insn(0x85, 0, 0, 0, 158), insn(0xb7, 0, 0, 0, 7),
		insn(0x95, 0, 0, 0, 0),
	};
	CHECK(reachable_instructions(joined, rodata(off))[6]);
	// A helper call clobbers r1-r5: r6 keeps the constant, r1 does not.
	std::vector<ebpf_inst> clobbered{
		insn(0x18, 1, 2, 0, RODATA_FD), insn(0, 0, 0, 0, 0),
		insn(0x71, 6, 1, 0, 0),	    insn(0xbf, 1, 6, 0, 0),
		insn(0x85, 0, 0, 0, 5),	    // r1 lost
		insn(0x15, 1, 0, 1, 0),	    insn(0x85, 0, 0, 0, 158),
		insn(0x15, 6, 0, 1, 0),	    insn(0x85, 0, 0, 0, 158),
		insn(0xb7, 0, 0, 0, 0),	    insn(0x95, 0, 0, 0, 0),
	};
	auto kept = reachable_instructions(clobbered, rodata(off));
	CHECK(kept[6]);
	CHECK_FALSE(kept[8]);
}

// libbpf-tools keep their options in .rodata (a frozen BPF_F_RDONLY_PROG
// array); the code of an option that is off may call helpers bpftime does not
// have (opensnoop's full_path: bpf_get_current_task_btf; syscount's -P:
// bpf_get_current_task). The kernel's verifier drops that code; bpftime
// disabled the whole program.
TEST_CASE("A missing helper behind a read-only constant does not disable the "
	  "program",
	  "[jit_load][reachability]")
{
	const char *old_name = getenv("BPFTIME_GLOBAL_SHM_NAME");
	const bool had_name = old_name != nullptr;
	const std::string previous_name = old_name ? old_name : "";
	const std::string name = "ReachabilityShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(name) };
	bool initialized = false;
	BOOST_SCOPE_EXIT_ALL(&)
	{
		if (initialized) {
			bpftime_destroy_global_shm();
			bpftime_initialize_global_shm(
				bpftime::shm_open_type::SHM_NO_CREATE);
		}
		if (had_name)
			setenv("BPFTIME_GLOBAL_SHM_NAME", previous_name.c_str(),
			       1);
		else
			unsetenv("BPFTIME_GLOBAL_SHM_NAME");
	};
	REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
	bpftime_destroy_global_shm();
	bpftime_initialize_global_shm(
		bpftime::shm_open_type::SHM_REMOVE_AND_CREATE);
	initialized = true;

	auto make_rodata = [](const char *map_name, uint64_t flags) {
		int fd = bpftime_maps_create(-1, map_name,
					     { .type = BPF_MAP_TYPE_ARRAY,
					       .key_size = 4,
					       .value_size = 8,
					       .max_ents = 1,
					       .flags = flags });
		REQUIRE(fd >= 0);
		const uint32_t key = 0;
		const uint64_t value = 0; // the option is off
		REQUIRE(bpftime_map_update_elem(fd, &key, &value, 0) == 0);
		return fd;
	};
	auto program = [](int fd) {
		auto code = option_guarded_call();
		code[0].imm = fd;
		return code;
	};
	uint64_t memory[4] = {};
	uint64_t ret = 0;

	const int rodata_fd = make_rodata("rodata", BPF_F_RDONLY_PROG);
	REQUIRE(bpftime_map_freeze(rodata_fd) == 0);
	// Frozen: the syscall can no longer change it, as the kernel.
	const uint32_t key = 0;
	const uint64_t on = 1;
	REQUIRE(bpftime_map_update_elem(rodata_fd, &key, &on, 0) == -1);
	REQUIRE(errno == EPERM);
	REQUIRE(bpftime_map_delete_elem(rodata_fd, &key) == -1);
	REQUIRE(errno == EPERM);
	REQUIRE(bpftime_map_freeze(rodata_fd) == -1);
	REQUIRE(errno == EBUSY);
	auto code = program(rodata_fd);
	bpftime_prog prog(code.data(), code.size(), "option_off",
			  runtime_config{});
	REQUIRE(prog.bpftime_prog_load(true) == 0);
	REQUIRE(prog.bpftime_prog_exec(memory, sizeof(memory), &ret) == 0);
	REQUIRE(ret == 7);

	// A kfunc call there (BPF_PSEUDO_KFUNC_CALL, imm a BTF id, which
	// nobody registers) is dropped the same way (review 2026-11-05).
	auto kfunc = program(rodata_fd);
	kfunc[4].src_reg = 2;
	kfunc[4].imm = 61234;
	bpftime_prog kfunc_prog(kfunc.data(), kfunc.size(), "kfunc_option_off",
				runtime_config{});
	REQUIRE(kfunc_prog.bpftime_prog_load(true) == 0);
	REQUIRE(kfunc_prog.bpftime_prog_exec(memory, sizeof(memory), &ret) == 0);
	REQUIRE(ret == 7);

	// Not frozen, or not read-only to programs: not a constant, the call
	// can run, and the program stays disabled.
	const int open_fd = make_rodata("open_rodata", BPF_F_RDONLY_PROG);
	const int writable_fd = make_rodata("writable", 0);
	REQUIRE(bpftime_map_freeze(writable_fd) == 0);
	for (int fd : { open_fd, writable_fd }) {
		INFO("map fd " << fd);
		auto other = program(fd);
		bpftime_prog disabled(other.data(), other.size(), "not_constant",
				      runtime_config{});
		REQUIRE(disabled.bpftime_prog_load(true) == 0);
		REQUIRE(disabled.bpftime_prog_exec(memory, sizeof(memory),
						   &ret) == -ENOEXEC);
	}
}

// The kernel drops write access to a frozen map for every syscall command:
// push, pop and lookup-and-delete were still allowed here (review
// 2026-11-05). A program's own writes are not syscall writes.
TEST_CASE("A frozen map refuses every syscall write", "[jit_load][reachability]")
{
	const char *old_name = getenv("BPFTIME_GLOBAL_SHM_NAME");
	const bool had_name = old_name != nullptr;
	const std::string previous_name = old_name ? old_name : "";
	const std::string name = "FrozenWritesShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(name) };
	bool initialized = false;
	BOOST_SCOPE_EXIT_ALL(&)
	{
		if (initialized) {
			bpftime_destroy_global_shm();
			bpftime_initialize_global_shm(
				bpftime::shm_open_type::SHM_NO_CREATE);
		}
		if (had_name)
			setenv("BPFTIME_GLOBAL_SHM_NAME", previous_name.c_str(),
			       1);
		else
			unsetenv("BPFTIME_GLOBAL_SHM_NAME");
	};
	REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
	bpftime_destroy_global_shm();
	bpftime_initialize_global_shm(
		bpftime::shm_open_type::SHM_REMOVE_AND_CREATE);
	initialized = true;

	const int queue = bpftime_maps_create(-1, "frozen_queue",
					      { .type = BPF_MAP_TYPE_QUEUE,
						.key_size = 0,
						.value_size = 8,
						.max_ents = 4 });
	REQUIRE(queue >= 0);
	uint64_t value = 5;
	REQUIRE(bpftime_map_push_elem(queue, &value, 0) == 0);
	REQUIRE(bpftime_map_freeze(queue) == 0);
	REQUIRE(bpftime_is_map_frozen(queue) == 1);
	CHECK(bpftime_map_push_elem(queue, &value, 0) == -1);
	CHECK(errno == EPERM);
	CHECK(bpftime_map_pop_elem(queue, &value) == -1);
	CHECK(errno == EPERM);
	CHECK(bpftime_map_lookup_and_delete_elem(queue, nullptr, &value) == -1);
	CHECK(errno == EPERM);

	const int hash = bpftime_maps_create(-1, "frozen_hash",
					     { .type = BPF_MAP_TYPE_HASH,
					       .key_size = 4,
					       .value_size = 8,
					       .max_ents = 4 });
	REQUIRE(hash >= 0);
	const uint32_t key = 1;
	REQUIRE(bpftime_map_update_elem(hash, &key, &value, 0) == 0);
	REQUIRE(bpftime_is_map_frozen(hash) == 0);
	REQUIRE(bpftime_map_freeze(hash) == 0);
	CHECK(bpftime_map_lookup_and_delete_elem(hash, &key, &value) == -1);
	CHECK(errno == EPERM);
	CHECK(bpftime_map_lookup_elem(hash, &key) != nullptr); // still there
}

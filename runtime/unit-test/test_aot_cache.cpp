/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026, eunomia-bpf org
 * All rights reserved.
 */
// The AOT cache (bpftime_prog_cache.cpp): the syscall server compiles each
// program once, when it is loaded, and agents link that object instead of
// compiling the program in every traced process.
#include "bpf_attach_ctx.hpp"
#include "bpftime_config.hpp"
#include "bpftime_prog.hpp"
#include "bpftime_prog_cache.hpp"
#include "bpftime_shm.hpp"
#include "bpftime_shm_internal.hpp"
#include "frida_attach_private_data.hpp"
#include "frida_uprobe_attach_impl.hpp"
#include "handler/prog_handler.hpp"
#include <boost/interprocess/shared_memory_object.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <linux/bpf.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

using namespace bpftime;

namespace
{
struct isolated_shm {
	std::string name = "bpftime-aot-cache-" + std::to_string(getpid());
	bool had_name = getenv("BPFTIME_GLOBAL_SHM_NAME") != nullptr;
	std::string previous = had_name ? getenv("BPFTIME_GLOBAL_SHM_NAME") : "";
	isolated_shm()
	{
		bpftime_destroy_global_shm();
		REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
		bpftime_initialize_global_shm(
			shm_open_type::SHM_REMOVE_AND_CREATE);
		runtime_config config;
		config.set_vm_name("llvm");
		shm_holder.global_shared_memory.set_runtime_config(
			std::move(config));
	}
	~isolated_shm()
	{
		unsetenv("BPFTIME_AOT_CACHE");
		shm_holder.global_shared_memory.reset_server_state();
		bpftime_destroy_global_shm();
		boost::interprocess::shared_memory_object::remove(name.c_str());
		bpftime_initialize_global_shm(shm_open_type::SHM_NO_CREATE);
		if (had_name)
			setenv("BPFTIME_GLOBAL_SHM_NAME", previous.c_str(), 1);
		else
			unsetenv("BPFTIME_GLOBAL_SHM_NAME");
	}
};

// r0 = bpf_get_current_pid_tgid() >> 32: the helper is linked by index, and
// the result names the process that ran it.
const ebpf_inst pid_program[] = {
	{ 0x85, 0, 0, 0, BPF_FUNC_get_current_pid_tgid },
	{ 0x77, 0, 0, 0, 32 },
	{ 0x95, 0, 0, 0, 0 },
};
// r0 = 7
const ebpf_inst seven_program[] = {
	{ 0xb7, 0, 0, 0, 7 },
	{ 0x95, 0, 0, 0, 0 },
};

// Writable, as tools/aot writes aot_insns.
bpf_prog_handler &prog_handler(int id)
{
	return const_cast<bpf_prog_handler &>(std::get<bpf_prog_handler>(
		shm_holder.global_shared_memory.get_manager()->get_handler(id)));
}

// The agent's uprobe impl, which registers the custom helpers.
void register_frida(bpf_attach_ctx &ctx)
{
	ctx.register_attach_impl(
		{ attach::ATTACH_UPROBE, attach::ATTACH_URETPROBE,
		  attach::ATTACH_UPROBE_OVERRIDE, attach::ATTACH_UREPLACE },
		std::make_unique<attach::frida_attach_impl>(),
		[](const std::string_view &sv, int &err) {
			std::unique_ptr<attach::attach_private_data> priv_data =
				std::make_unique<
					attach::frida_attach_private_data>();
			if (int e = priv_data->initialize_from_string(sv);
			    e < 0) {
				err = e;
				return std::unique_ptr<
					attach::attach_private_data>();
			}
			return priv_data;
		});
}

uint64_t run(bpftime_prog *prog)
{
	uint64_t memory[4] = {};
	uint64_t ret = 0;
	REQUIRE(prog->bpftime_prog_exec(memory, sizeof(memory), &ret) == 0);
	return ret;
}

struct child_result {
	int aot = -1;
	uint64_t ret = 0;
	uintptr_t code = 0;
	pid_t pid = 0;
};

// Runs program id in a new process image (the child test case below): its
// code is at other addresses than this process's (ASLR), as in a traced
// process. frida says whether it registers the uprobe impl.
child_result run_in_child(int id, bool frida = true)
{
	int out[2];
	REQUIRE(pipe(out) == 0);
	pid_t pid = fork();
	REQUIRE(pid >= 0);
	if (pid == 0) {
		dup2(out[1], STDOUT_FILENO);
		close(out[0]);
		close(out[1]);
		setenv("BPFTIME_AOT_CACHE_CHILD_PROG", std::to_string(id).c_str(),
		       1);
		if (!frida)
			setenv("BPFTIME_AOT_CACHE_CHILD_NO_FRIDA", "1", 1);
		setenv("SPDLOG_LEVEL", "off", 1);
		execl("/proc/self/exe", "bpftime_runtime_tests",
		      "[.aot-cache-child]", "-o", "/dev/null", nullptr);
		_exit(127);
	}
	close(out[1]);
	std::string text;
	char buf[256];
	ssize_t n;
	while ((n = read(out[0], buf, sizeof(buf))) > 0)
		text.append(buf, n);
	close(out[0]);
	int status = 0;
	REQUIRE(waitpid(pid, &status, 0) == pid);
	REQUIRE(WIFEXITED(status));
	REQUIRE(WEXITSTATUS(status) == 0);
	child_result result;
	result.pid = pid;
	auto at = text.find("aot-cache-child ");
	REQUIRE(at != std::string::npos);
	unsigned long long ret = 0, code = 0;
	REQUIRE(sscanf(text.c_str() + at,
		       "aot-cache-child aot=%d ret=%llu code=%llx", &result.aot,
		       &ret, &code) == 3);
	result.ret = ret;
	result.code = code;
	return result;
}
} // namespace

TEST_CASE("Agents link the AOT object the server compiled for a program",
	  "[aot-cache]")
{
	isolated_shm fixture;
	int id = bpftime_progs_create_cached(-1, pid_program, 3, "aot_pid",
					     BPF_PROG_TYPE_SOCKET_FILTER);
	REQUIRE(id >= 0);
	REQUIRE(!prog_handler(id).aot_insns.empty());
	REQUIRE(!prog_handler(id).aot_key.empty());

	// Another process image: the helpers are linked at its own addresses.
	auto child = run_in_child(id);
	CHECK(child.code != reinterpret_cast<uintptr_t>(&run_in_child));
	REQUIRE(child.aot == 1);
	REQUIRE(child.ret == static_cast<uint64_t>(child.pid));

	// And in this process.
	bpf_attach_ctx ctx;
	register_frida(ctx);
	REQUIRE(ctx.init_attach_ctx_from_handlers(bpftime_get_runtime_config()) ==
		0);
	auto *prog = ctx.find_instantiated_prog(id);
	REQUIRE(prog != nullptr);
	REQUIRE(prog->is_loaded_from_aot());
	REQUIRE(run(prog) == static_cast<uint64_t>(getpid()));
}

TEST_CASE("A program that differs from what its AOT object was compiled from "
	  "is compiled",
	  "[aot-cache]")
{
	isolated_shm fixture;
	int seven = bpftime_progs_create_cached(-1, seven_program, 2, "seven",
						BPF_PROG_TYPE_SOCKET_FILTER);
	int pid = bpftime_progs_create_cached(-1, pid_program, 3, "aot_pid",
					      BPF_PROG_TYPE_SOCKET_FILTER);
	REQUIRE(seven >= 0);
	REQUIRE(pid >= 0);

	SECTION("other instructions")
	{
		// seven's handler holds the pid program's object and key.
		auto &handler = prog_handler(seven);
		handler.aot_insns = prog_handler(pid).aot_insns;
		handler.aot_key = prog_handler(pid).aot_key;
		auto child = run_in_child(seven);
		REQUIRE(child.aot == 0);
		REQUIRE(child.ret == 7);
	}
	SECTION("other helpers")
	{
		// An agent without the uprobe impl's custom helpers.
		auto child = run_in_child(pid, false);
		REQUIRE(child.aot == 0);
		REQUIRE(child.ret == static_cast<uint64_t>(child.pid));
	}
}

TEST_CASE("The server caches only programs whose code holds no addresses of "
	  "its own process",
	  "[aot-cache]")
{
	isolated_shm fixture;
	constexpr int map_fd = 100;
	REQUIRE(bpftime_maps_create(map_fd, "aot_cache_array",
				    { .type = BPF_MAP_TYPE_ARRAY,
				      .key_size = 4,
				      .value_size = 8,
				      .max_ents = 1 }) == map_fd);
	SECTION("global data")
	{
		// r0 = *(u64 *)(map value + 0)
		const ebpf_inst code[] = {
			{ 0x18, 1, BPF_PSEUDO_MAP_VALUE, 0, map_fd },
			{ 0x00, 0, 0, 0, 0 },
			{ 0x79, 0, 1, 0, 0 },
			{ 0x95, 0, 0, 0, 0 },
		};
		int id = bpftime_progs_create_cached(
			-1, code, 4, "global_data", BPF_PROG_TYPE_SOCKET_FILTER);
		REQUIRE(id >= 0);
		REQUIRE(prog_handler(id).aot_insns.empty());
		REQUIRE(prog_handler(id).aot_key.empty());
	}
	SECTION("a bound array lookup")
	{
		// r0 = bpf_map_lookup_elem(map, &(u32)0)
		const ebpf_inst code[] = {
			{ 0xb7, 1, 0, 0, 0 },
			{ 0x63, 10, 1, -4, 0 },
			{ 0xbf, 2, 10, 0, 0 },
			{ 0x07, 2, 0, 0, -4 },
			{ 0x18, 1, BPF_PSEUDO_MAP_FD, 0, map_fd },
			{ 0x00, 0, 0, 0, 0 },
			{ 0x85, 0, 0, 0, BPF_FUNC_map_lookup_elem },
			{ 0x95, 0, 0, 0, 0 },
		};
		int id = bpftime_progs_create_cached(
			-1, code, 8, "bound_lookup", BPF_PROG_TYPE_SOCKET_FILTER);
		REQUIRE(id >= 0);
		REQUIRE(prog_handler(id).aot_insns.empty());
	}
	SECTION("BPFTIME_AOT_CACHE=0")
	{
		REQUIRE(setenv("BPFTIME_AOT_CACHE", "0", 1) == 0);
		int id = bpftime_progs_create_cached(
			-1, seven_program, 2, "seven", BPF_PROG_TYPE_SOCKET_FILTER);
		REQUIRE(id >= 0);
		REQUIRE(prog_handler(id).aot_insns.empty());
	}
}

// Run by run_in_child in a new process image: instantiates the programs of
// the shared memory as an agent does and runs the one named by
// BPFTIME_AOT_CACHE_CHILD_PROG.
TEST_CASE("AOT cache child", "[.aot-cache-child]")
{
	const char *id_text = getenv("BPFTIME_AOT_CACHE_CHILD_PROG");
	if (id_text == nullptr)
		SKIP("only run by the [aot-cache] tests");
	bpftime_destroy_global_shm();
	bpftime_initialize_global_shm(shm_open_type::SHM_OPEN_ONLY);
	bpf_attach_ctx ctx;
	if (getenv("BPFTIME_AOT_CACHE_CHILD_NO_FRIDA") == nullptr)
		register_frida(ctx);
	REQUIRE(ctx.init_attach_ctx_from_handlers(bpftime_get_runtime_config()) ==
		0);
	auto *prog = ctx.find_instantiated_prog(atoi(id_text));
	REQUIRE(prog != nullptr);
	uint64_t ret = run(prog);
	printf("aot-cache-child aot=%d ret=%" PRIu64 " code=%llx\n",
	       prog->is_loaded_from_aot() ? 1 : 0, ret,
	       (unsigned long long)reinterpret_cast<uintptr_t>(&run_in_child));
	fflush(stdout);
}

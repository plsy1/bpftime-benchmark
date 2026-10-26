/* SPDX-License-Identifier: MIT */
// Tests that load the daemon's kernel tracer; they need root and skip
// otherwise.
#include <catch2/catch_test_macros.hpp>
#include <bpf/libbpf.h>
#include <memory>
#include <unistd.h>
#include "user/daemon.hpp"
#include "user/bpf_prog_insns.hpp"
#include "bpf_tracer.skel.h"

namespace
{
struct tracer_deleter {
	void operator()(bpf_tracer_bpf *obj) const
	{
		bpf_tracer_bpf__destroy(obj);
	}
};
using tracer_ptr = std::unique_ptr<bpf_tracer_bpf, tracer_deleter>;

// Opens the tracer filtered to this process, with the programs the daemon
// would select for ENV.
tracer_ptr open_tracer(const daemon_config &env)
{
	tracer_ptr obj(bpf_tracer_bpf__open());
	REQUIRE(obj);
	obj->rodata->target_pid = getpid();
	bpftime::select_tracer_programs(obj.get(), env);
	return obj;
}
} // namespace

TEST_CASE("Daemon tracer attaches without the syscalls this kernel lacks",
	  "[daemon-tracer]")
{
	if (geteuid() != 0)
		SKIP("loading the tracer needs root");
	daemon_config env;
	// -o adds sys_enter_open/sys_exit_open, which arm64 and riscv64 do not
	// have; attaching them used to fail the whole tracer there.
	env.show_open = true;
	auto obj = open_tracer(env);
	REQUIRE(bpf_tracer_bpf__load(obj.get()) == 0);
	REQUIRE(bpf_tracer_bpf__attach(obj.get()) == 0);
	REQUIRE(bpf_program__autoload(
		obj->progs.tracepoint__syscalls__sys_enter_openat));
	REQUIRE(bpf_program__autoload(
		obj->progs.tracepoint__syscalls__sys_enter_bpf));
}

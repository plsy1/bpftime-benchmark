#include "bpftime_prog.hpp"
#include <bpftime_shm.hpp>
#include <bpftime_shm_internal.hpp>
#include <boost/scope_exit.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <unistd.h>

using namespace bpftime;

// Probes stay attached until the process ends, while the exit-time destructor
// unmaps the global shm. A hooked call from a later destructor (memleak's
// free probe, found on Jetson) then ran its program against unmapped maps.
TEST_CASE("Programs are skipped after the global shm is released",
	  "[shm_release]")
{
	const char *old_name = getenv("BPFTIME_GLOBAL_SHM_NAME");
	const std::string previous_name = old_name ? old_name : "";
	const std::string name =
		"ShmReleaseTestShm-" + std::to_string(getpid());
	BOOST_SCOPE_EXIT_ALL(&)
	{
		bpftime_destroy_global_shm();
		bpftime_initialize_global_shm(shm_open_type::SHM_NO_CREATE);
		bpftime_remove_global_shm();
		if (old_name)
			setenv("BPFTIME_GLOBAL_SHM_NAME", previous_name.c_str(),
			       1);
		else
			unsetenv("BPFTIME_GLOBAL_SHM_NAME");
	};
	REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
	bpftime_destroy_global_shm();
	bpftime_initialize_global_shm(shm_open_type::SHM_REMOVE_AND_CREATE);

	const ebpf_inst code[] = {
		{ .code = 0xb7, .dst_reg = 0, .src_reg = 0, .off = 0, .imm = 42 },
		{ .code = 0x95, .dst_reg = 0, .src_reg = 0, .off = 0, .imm = 0 },
	};
	bpftime_prog prog(code, 2, "after_shm_release");
	REQUIRE(prog.bpftime_prog_load(false) == 0);
	uint64_t memory[4] = {};
	uint64_t ret = 0;
	REQUIRE(prog.bpftime_prog_exec(memory, sizeof(memory), &ret) == 0);
	REQUIRE(ret == 42);

	bpftime_destroy_global_shm();
	ret = 7;
	REQUIRE(prog.bpftime_prog_exec(memory, sizeof(memory), &ret) ==
		-ENODEV);
	REQUIRE(ret == 0);

	// A new segment (tests, re-initialization) runs programs again.
	bpftime_initialize_global_shm(shm_open_type::SHM_REMOVE_AND_CREATE);
	REQUIRE(prog.bpftime_prog_exec(memory, sizeof(memory), &ret) == 0);
	REQUIRE(ret == 42);
}

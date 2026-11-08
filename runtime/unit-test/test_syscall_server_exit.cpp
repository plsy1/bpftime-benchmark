/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026, eunomia-bpf org
 * All rights reserved.
 */
#include "catch2/catch_test_macros.hpp"

#if defined(__linux__)
#include "shm_allocation_test_paths.hpp"

#include <boost/interprocess/shared_memory_object.hpp>
#include <cerrno>
#include <cstdlib>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

// At exit the syscall server's __destruct_shm (an ELF destructor) unmaps the
// segment while destructors of libraries finalized after it still call
// interposed functions. Under CUDA, libnvrm_mem's closes /dev/nvmap, and
// close() took the handler path on the unmapped segment: every loader died
// with SIGSEGV (results gpu/examples-20261109, problem 9). The helper's
// library closes its bpftime map fd in its destructor the same way.
TEST_CASE("The syscall server passes calls made after its segment is gone to libc",
	  "[syscall-server-exit]")
{
	const std::string shm_name =
		"bpftime-close-at-exit-test-" + std::to_string(getpid());
	boost::interprocess::shared_memory_object::remove(shm_name.c_str());
	int output_pipe[2];
	REQUIRE(pipe(output_pipe) == 0);
	pid_t pid = fork();
	REQUIRE(pid >= 0);
	if (pid == 0) {
		close(output_pipe[0]);
		if (dup2(output_pipe[1], STDOUT_FILENO) == -1 ||
		    dup2(output_pipe[1], STDERR_FILENO) == -1)
			_exit(125);
		close(output_pipe[1]);
		if (setenv("BPFTIME_LOG_OUTPUT", "console", 1) != 0 ||
		    setenv("BPFTIME_GLOBAL_SHM_NAME", shm_name.c_str(), 1) !=
			    0 ||
		    setenv("LD_PRELOAD", BPFTIME_SYSCALL_SERVER_LIBRARY, 1) != 0)
			_exit(126);
		execl(BPFTIME_CLOSE_AT_EXIT_TEST_HELPER,
		      BPFTIME_CLOSE_AT_EXIT_TEST_HELPER, nullptr);
		_exit(127);
	}
	close(output_pipe[1]);
	std::string output;
	char buffer[4096];
	for (;;) {
		ssize_t count = read(output_pipe[0], buffer, sizeof(buffer));
		if (count > 0) {
			output.append(buffer, static_cast<size_t>(count));
			continue;
		}
		if (count == -1 && errno == EINTR)
			continue;
		break;
	}
	close(output_pipe[0]);
	int status = 0;
	while (waitpid(pid, &status, 0) == -1)
		REQUIRE(errno == EINTR);
	boost::interprocess::shared_memory_object::remove(shm_name.c_str());
	INFO(output);
	// The server created the map (the helper checks its segment).
	REQUIRE(output.find("map fd") != std::string::npos);
	// Killed by SIGSEGV before.
	REQUIRE(WIFEXITED(status));
	REQUIRE(WEXITSTATUS(status) == 0);
}
#endif

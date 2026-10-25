#include "bpftime_logger.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

extern "C" uint64_t bpftime_trace_printk(uint64_t fmt, uint64_t fmt_size, ...);

// Runs in a fresh process (the sink is resolved once per process).
TEST_CASE("bpf_trace_printk child", "[.][trace_printk_child]")
{
	if (const char *log = getenv("PRINTK_TEST_LOG"))
		bpftime::bpftime_set_logger(log); // as the agent does at start-up
	const char fmt[] = "hello %d\n";
	REQUIRE(bpftime_trace_printk((uint64_t)fmt, sizeof(fmt), 42) ==
		(getenv("PRINTK_TEST_EXPECT_ZERO") ? 0 : 9));
	spdlog::default_logger()->flush();
}

namespace
{
std::string slurp(const std::string &path)
{
	std::ifstream in(path);
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

// stdout of the child test case with BPFTIME_TRACE_PRINTK_OUTPUT = target
// (unset when null).
std::string run_child(const char *target, const std::string &log,
		      bool expect_zero = false)
{
	int out[2];
	REQUIRE(pipe(out) == 0);
	pid_t pid = fork();
	REQUIRE(pid >= 0);
	if (pid == 0) {
		dup2(out[1], STDOUT_FILENO);
		close(out[0]);
		close(out[1]);
		if (target)
			setenv("BPFTIME_TRACE_PRINTK_OUTPUT", target, 1);
		else
			unsetenv("BPFTIME_TRACE_PRINTK_OUTPUT");
		setenv("PRINTK_TEST_LOG", log.c_str(), 1);
		if (expect_zero)
			setenv("PRINTK_TEST_EXPECT_ZERO", "1", 1);
		execl("/proc/self/exe", "bpftime_runtime_tests",
		      "[trace_printk_child]", "-o", "/dev/null", nullptr);
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
	return text;
}
} // namespace

// The injected-agent contract: bpf_trace_printk does not write the host's
// stdout unless chosen (it used to vprintf into it).
TEST_CASE("bpf_trace_printk stays off host stdout unless selected",
	  "[trace_printk]")
{
	const std::string dir = "/tmp/bpftime-printk-test-" +
				std::to_string(getpid());
	std::filesystem::create_directories(dir);
	const std::string log = dir + "/runtime.log";
	const std::string file = dir + "/printk.txt";

	// Default: a bpftime log record, nothing on stdout.
	// (The test binary's own start-up logging may reach stdout.)
	REQUIRE(run_child(nullptr, log).find("hello 42") == std::string::npos);
	REQUIRE(slurp(log).find("bpf_trace_printk: hello 42") !=
		std::string::npos);

	REQUIRE(run_child("stdout", log).find("hello 42\n") != std::string::npos);

	REQUIRE(run_child(file.c_str(), log).find("hello 42") == std::string::npos);
	REQUIRE(slurp(file) == "hello 42\n");

	REQUIRE(run_child("none", log, true).find("hello 42") == std::string::npos);
	REQUIRE(slurp(file) == "hello 42\n");

	std::filesystem::remove_all(dir);
}

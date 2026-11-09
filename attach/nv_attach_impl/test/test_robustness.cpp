// Side effects of CUDA attach on the application, and how it fails when the
// GPU cannot be queried.
#include "catch2/catch_test_macros.hpp"
#include "nv_attach_impl.hpp"
#include "nv_attach_utils.hpp"
#include "ptx_compiler/ptx_compiler.hpp"
#include "ptx_pass_config.h"
#include <clocale>
#include <cstdlib>
#include <cstring>
#include <spawn.h>
#include <spdlog/spdlog.h>
#include <string>
#include <sys/wait.h>
#include <vector>

extern char **environ;

using namespace bpftime::attach;

namespace
{
struct env_restorer {
	std::string name;
	bool had;
	std::string previous;
	explicit env_restorer(const char *name)
		: name(name), had(getenv(name) != nullptr),
		  previous(had ? getenv(name) : "")
	{
	}
	~env_restorer()
	{
		if (had)
			setenv(name.c_str(), previous.c_str(), 1);
		else
			unsetenv(name.c_str());
	}
};
} // namespace

// The detected architecture used to be stored in the application's
// environment with setenv.
TEST_CASE("Detecting the GPU's SM architecture leaves the environment alone",
	  "[sm-arch]")
{
	env_restorer arch("BPFTIME_SM_ARCH");
	unsetenv("BPFTIME_SM_ARCH");
	const auto detected = get_gpu_sm_arch();
	if (detected.empty())
		SKIP("no CUDA device");
	CHECK(detected.rfind("sm_", 0) == 0);
	CHECK(getenv("BPFTIME_SM_ARCH") == nullptr);
	CHECK(get_gpu_sm_arch() == detected);
	setenv("BPFTIME_SM_ARCH", "sm_75", 1);
	CHECK(get_gpu_sm_arch() == "sm_75");
}

// Run by the next test in a child without visible devices.
TEST_CASE("SM architecture without a device", "[.sm-arch-without-device]")
{
	REQUIRE(getenv("BPFTIME_SM_ARCH") == nullptr);
	CHECK(get_gpu_sm_arch().empty());
}

// When the driver could not be queried the answer was sm_61, which CUDA 13
// refuses (and is wrong for any newer device): compiling then failed deep in
// the PTX compiler. Now there is no answer, and the kernels stay unpatched.
TEST_CASE("Without a usable GPU there is no SM architecture", "[sm-arch]")
{
	std::vector<std::string> env_strings;
	for (char **e = environ; *e != nullptr; e++) {
		if (strncmp(*e, "BPFTIME_SM_ARCH=", 16) == 0 ||
		    strncmp(*e, "CUDA_VISIBLE_DEVICES=", 21) == 0)
			continue;
		env_strings.emplace_back(*e);
	}
	env_strings.emplace_back("CUDA_VISIBLE_DEVICES=");
	std::vector<char *> envp;
	for (auto &s : env_strings)
		envp.push_back(s.data());
	envp.push_back(nullptr);
	std::string exe = "/proc/self/exe", filter = "[.sm-arch-without-device]";
	char *argv[] = { exe.data(), filter.data(), nullptr };
	pid_t pid;
	REQUIRE(posix_spawn(&pid, exe.c_str(), nullptr, nullptr, argv,
			    envp.data()) == 0);
	int status = 0;
	REQUIRE(waitpid(pid, &status, 0) == pid);
	CHECK(WIFEXITED(status));
	CHECK(WEXITSTATUS(status) == 0);
}

// The extracted and patched PTX were kept in /tmp/bpftime-fatbin-work.*
// whenever debug logging was on: one directory per fatbin and run, never
// removed (90 on the board after a day of debug runs).
TEST_CASE("PTX work directories are kept only on request", "[ptx-work-dir]")
{
	env_restorer keep("BPFTIME_CUDA_KEEP_PTX");
	const auto level = spdlog::get_level();
	spdlog::set_level(spdlog::level::debug);
	unsetenv("BPFTIME_CUDA_KEEP_PTX");
	CHECK_FALSE(nv_attach_impl::keeps_ptx_work_dirs());
	setenv("BPFTIME_CUDA_KEEP_PTX", "1", 1);
	CHECK(nv_attach_impl::keeps_ptx_work_dirs());
	spdlog::set_level(spdlog::level::info);
	CHECK(nv_attach_impl::keeps_ptx_work_dirs());
	spdlog::set_level(level);
}

// Creating the compiler called setlocale(LC_ALL, ""): under LD_PRELOAD the
// agent and the compiler library share the application's libc, so the whole
// application switched to its environment's locale (number formatting,
// character classes).
TEST_CASE("Compiling PTX leaves the application's locale alone",
	  "[ptx-compiler]")
{
	void *handle = nullptr;
	auto compiler = bpftime::load_nv_attach_impl_ptx_compiler(
		DEFAULT_PTX_COMPILER_SHARED_LIB, handle);
	REQUIRE(compiler);
	env_restorer lc_all("LC_ALL");
	setenv("LC_ALL", "C.UTF-8", 1);
	const std::string before = setlocale(LC_ALL, nullptr);
	REQUIRE(before == "C");

	auto *c = compiler->create();
	const char *ptx = ".version 7.4\n.target sm_75\n.address_size 64\n"
			  ".visible .entry bpftime_locale_test()\n{\n\tret;\n}\n";
	const char *args[] = { "--gpu-name=sm_75" };
	CHECK(compiler->compile(c, ptx, args, 1) == 0);
	compiler->destroy(c);
	CHECK(std::string(setlocale(LC_ALL, nullptr)) == before);
}

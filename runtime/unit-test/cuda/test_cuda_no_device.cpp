// A CUDA build's agent in a process without a usable GPU. Without a device
// address for CommSharedMem, CUDAContext threw out of bpf_attach_ctx's
// constructor and the agent disabled itself, CPU probes included
// (funclatency under build-cuda with CUDA_VISIBLE_DEVICES= counted 0 of
// 1,123,346,000 calls; results gpu/fixes-20261109/verification).
#include "bpf_attach_ctx.hpp"
#include "bpftime_config.hpp"
#include "bpftime_prog.hpp"
#include "bpftime_shm.hpp"
#include "bpftime_shm_internal.hpp"
#include "cuda.h"
#include <boost/interprocess/shared_memory_object.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <linux/bpf.h>
#include <optional>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

using namespace bpftime;

namespace
{
// r0 = 42
const ebpf_inst cpu_program[] = {
	{ 0xb7, 0, 0, 0, 42 },
	{ 0x95, 0, 0, 0, 0 },
};
} // namespace

// Run by the next test in a process image of its own, with no visible CUDA
// device: opens the segment as an agent does (its registration with CUDA
// fails at cuInit), makes the attach context and runs the program.
TEST_CASE("Attach context without a usable GPU (child)",
	  "[.cuda-no-device-child]")
{
	const char *id_text = getenv("BPFTIME_CUDA_NO_DEVICE_CHILD_PROG");
	if (id_text == nullptr)
		SKIP("only run by [cuda-no-device]");
	bpftime_destroy_global_shm();
	bpftime_initialize_global_shm(shm_open_type::SHM_OPEN_ONLY);
	const bool no_gpu = cuInit(0) != CUDA_SUCCESS;
	std::optional<bpf_attach_ctx> ctx;
	REQUIRE_NOTHROW(ctx.emplace());
	REQUIRE(ctx->init_attach_ctx_from_handlers(
			bpftime_get_runtime_config()) == 0);
	auto *prog = ctx->find_instantiated_prog(atoi(id_text));
	REQUIRE(prog != nullptr);
	uint64_t memory[4] = {};
	uint64_t ret = 0;
	REQUIRE(prog->bpftime_prog_exec(memory, sizeof(memory), &ret) == 0);
	printf("cuda-no-device-child no_gpu=%d ret=%" PRIu64
	       " device_pointer=%" PRIuPTR "\n",
	       no_gpu ? 1 : 0, ret,
	       static_cast<uintptr_t>(
		       cuda::cuda_shared_mem_device_pointer_storage().load()));
	fflush(stdout);
	ctx.reset();
}

TEST_CASE("Without a usable GPU the attach context runs without CUDA attach",
	  "[cuda-no-device]")
{
	const std::string name =
		"bpftime-cuda-no-device-" + std::to_string(getpid());
	const char *had = getenv("BPFTIME_GLOBAL_SHM_NAME");
	const std::string previous = had ? had : "";
	bpftime_destroy_global_shm();
	REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
	// The syscall server's side: the segment with CommSharedMem and a
	// CPU program.
	bpftime_initialize_global_shm(shm_open_type::SHM_REMOVE_AND_CREATE);
	runtime_config config;
	config.set_vm_name("llvm");
	shm_holder.global_shared_memory.set_runtime_config(std::move(config));
	const int id = bpftime_progs_create(-1, cpu_program,
					    std::size(cpu_program), "no_device",
					    BPF_PROG_TYPE_SOCKET_FILTER);
	REQUIRE(id >= 0);
	// A request another process's GPU has in flight: a process that
	// cannot serve it must leave it alone.
	auto *comm = shm_holder.global_shared_memory.get_cuda_comm_shared_mem();
	REQUIRE(comm != nullptr);
	comm->request_id = 1234;
	comm->flag1 = 1;

	int out[2];
	REQUIRE(pipe(out) == 0);
	pid_t pid = fork();
	REQUIRE(pid >= 0);
	if (pid == 0) {
		dup2(out[1], STDOUT_FILENO);
		close(out[0]);
		close(out[1]);
		setenv("BPFTIME_CUDA_NO_DEVICE_CHILD_PROG",
		       std::to_string(id).c_str(), 1);
		setenv("CUDA_VISIBLE_DEVICES", "", 1);
		setenv("SPDLOG_LEVEL", "off", 1);
		execl("/proc/self/exe", "bpftime_runtime_tests",
		      "[.cuda-no-device-child]", "-o", "/dev/null", nullptr);
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
	const int flag1 = comm->flag1;
	const int request_id = comm->request_id;

	shm_holder.global_shared_memory.reset_server_state();
	bpftime_destroy_global_shm();
	boost::interprocess::shared_memory_object::remove(name.c_str());
	bpftime_initialize_global_shm(shm_open_type::SHM_NO_CREATE);
	if (had)
		setenv("BPFTIME_GLOBAL_SHM_NAME", previous.c_str(), 1);
	else
		unsetenv("BPFTIME_GLOBAL_SHM_NAME");

	REQUIRE(WIFEXITED(status));
	REQUIRE(WEXITSTATUS(status) == 0);
	int no_gpu = -1;
	unsigned long long ret = 0, device_pointer = 1;
	auto at = text.find("cuda-no-device-child ");
	REQUIRE(at != std::string::npos);
	REQUIRE(sscanf(text.c_str() + at,
		       "cuda-no-device-child no_gpu=%d ret=%llu device_pointer=%llu",
		       &no_gpu, &ret, &device_pointer) == 3);
	CHECK(no_gpu == 1);
	CHECK(ret == 42);
	CHECK(device_pointer == 0);
	CHECK(flag1 == 1);
	CHECK(request_id == 1234);
}

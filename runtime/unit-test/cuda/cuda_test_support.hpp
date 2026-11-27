// Helpers of the CUDA unit tests (bpftime_runtime_tests), shared by
// test_cuda_host_call.cpp and test_gpu_devhash_device.cpp: the primary
// context, modules of test kernels (with the trampoline, or raw), device
// buffers, scratch segments, a host-call server that counts requests, and an
// agent-side global segment.
#ifndef BPFTIME_CUDA_TEST_SUPPORT_HPP
#define BPFTIME_CUDA_TEST_SUPPORT_HPP
#include "bpftime_shm.hpp"
#include "bpftime_shm_internal.hpp"
#include "catch2/catch_test_macros.hpp"
#include "cuda.h"
#include "nv_attach_impl.hpp"
#include "nv_attach_utils.hpp"
#include "trampoline/gpu_helper_protocol.h"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace cuda_test
{
using namespace bpftime;
using namespace boost::interprocess;

// The primary context of device 0, current on this thread; false without a
// usable GPU.
inline bool use_primary_context()
{
	if (cuInit(0) != CUDA_SUCCESS)
		return false;
	int count = 0;
	if (cuDeviceGetCount(&count) != CUDA_SUCCESS || count == 0)
		return false;
	CUdevice device;
	CUcontext primary;
	if (cuDeviceGet(&device, 0) != CUDA_SUCCESS ||
	    cuDevicePrimaryCtxRetain(&primary, device) != CUDA_SUCCESS)
		return false;
	return cuCtxSetCurrent(primary) == CUDA_SUCCESS;
}

inline std::string device_sm_arch()
{
	CUdevice device;
	int major = 0, minor = 0;
	REQUIRE(cuCtxGetDevice(&device) == CUDA_SUCCESS);
	cuDeviceGetAttribute(&major,
			     CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR,
			     device);
	cuDeviceGetAttribute(&minor,
			     CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR,
			     device);
	return "sm_" + std::to_string(major * 10 + minor);
}

// The trampoline and test kernels in one module, as bpftime builds a probed
// module; or (raw) a whole module's PTX loaded as it is.
struct test_module {
	CUmodule module = nullptr;

	explicit test_module(const char *kernels_ptx, bool raw = false)
	{
		const auto ptx =
			raw ? std::string(kernels_ptx) :
			      attach::wrap_ptx_with_trampoline_for_sm(
				      kernels_ptx, device_sm_arch());
		char log[8192] = {};
		CUjit_option options[] = { CU_JIT_ERROR_LOG_BUFFER,
					   CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES };
		void *values[] = { log, (void *)(uintptr_t)sizeof(log) };
		const auto err = cuModuleLoadDataEx(&module, ptx.c_str(), 2,
						    options, values);
		INFO("JIT log: " << log);
		REQUIRE(err == CUDA_SUCCESS);
	}
	~test_module()
	{
		cuModuleUnload(module);
	}
	void set_map_info(int index, const attach::MapBasicInfo &info)
	{
		CUdeviceptr map_info;
		size_t size;
		REQUIRE(cuModuleGetGlobal(&map_info, &size, module,
					  "map_info") == CUDA_SUCCESS);
		REQUIRE((index + 1) * sizeof(info) <= size);
		REQUIRE(cuMemcpyHtoD(map_info + index * sizeof(info), &info,
				     sizeof(info)) == CUDA_SUCCESS);
	}
	void set_comm(uintptr_t comm_device_pointer)
	{
		CUdeviceptr const_data;
		size_t size;
		REQUIRE(cuModuleGetGlobal(&const_data, &size, module,
					  "constData") == CUDA_SUCCESS);
		REQUIRE(cuMemcpyHtoD(const_data, &comm_device_pointer,
				     sizeof(comm_device_pointer)) ==
			CUDA_SUCCESS);
	}
	void launch(const char *kernel, unsigned blocks, unsigned threads,
		    std::vector<uint64_t> args)
	{
		CUfunction function;
		REQUIRE(cuModuleGetFunction(&function, module, kernel) ==
			CUDA_SUCCESS);
		std::vector<void *> params;
		for (auto &arg : args)
			params.push_back(&arg);
		REQUIRE(cuLaunchKernel(function, blocks, 1, 1, threads, 1, 1, 0,
				       nullptr, params.data(),
				       nullptr) == CUDA_SUCCESS);
		REQUIRE(cuCtxSynchronize() == CUDA_SUCCESS);
	}
};

struct device_buffer {
	CUdeviceptr ptr = 0;
	size_t size;
	explicit device_buffer(size_t size) : size(size)
	{
		REQUIRE(cuMemAlloc(&ptr, size) == CUDA_SUCCESS);
		REQUIRE(cuMemsetD8(ptr, 0, size) == CUDA_SUCCESS);
	}
	~device_buffer()
	{
		cuMemFree(ptr);
	}
	std::vector<uint64_t> read() const
	{
		std::vector<uint64_t> out(size / sizeof(uint64_t));
		REQUIRE(cuMemcpyDtoH(out.data(), ptr, size) == CUDA_SUCCESS);
		return out;
	}
};

struct scratch_segment {
	std::string name;
	managed_shared_memory segment;
	explicit scratch_segment(const char *what, size_t size)
		: name(std::string("bpftime-test-") + what + "-" +
		       std::to_string(getpid())),
		  segment((shared_memory_object::remove(name.c_str()),
			   create_only),
			  name.c_str(), size)
	{
	}
	~scratch_segment()
	{
		shared_memory_object::remove(name.c_str());
	}
};

// A request slot of its own (in a scratch segment registered with CUDA) that
// a thread of the test serves like the watcher, answering zeros and counting
// the requests: a module pointed at it shows which helpers still ask the host.
struct counting_host {
	scratch_segment scratch;
	bpftime_gpu::CommSharedMem *comm;
	CUdeviceptr comm_on_device = 0;
	std::atomic<bool> stop{ false };
	std::atomic<int> served{ 0 };
	std::thread server;
	counting_host()
		: scratch("counting-host", 8 << 20),
		  comm(scratch.segment.construct<bpftime_gpu::CommSharedMem>(
			  anonymous_instance)())
	{
		memset((void *)comm, 0, sizeof(*comm));
		REQUIRE(cuMemHostRegister(scratch.segment.get_address(),
					  scratch.segment.get_size(),
					  CU_MEMHOSTREGISTER_DEVICEMAP |
						  CU_MEMHOSTREGISTER_PORTABLE) ==
			CUDA_SUCCESS);
		REQUIRE(cuMemHostGetDevicePointer(&comm_on_device, comm, 0) ==
			CUDA_SUCCESS);
		server = std::thread([this]() {
			while (!stop.load(std::memory_order_acquire)) {
				int posted = 1;
				if (__atomic_load_n(&comm->flag1,
						    __ATOMIC_RELAXED) == 1 &&
				    __atomic_compare_exchange_n(
					    &comm->flag1, &posted, 0, false,
					    __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
					memset((void *)&comm->resp, 0,
					       sizeof(comm->resp));
					served++;
					__atomic_store_n(&comm->flag2, 1,
							 __ATOMIC_RELEASE);
				}
			}
		});
	}
	~counting_host()
	{
		stop.store(true, std::memory_order_release);
		server.join();
		cuMemHostUnregister(scratch.segment.get_address());
	}
};

// The agent's side of a test: the global segment created as the syscall
// server does (make_maps creates the maps in it), then opened as an agent.
struct agent_segment {
	std::string name;
	std::string previous;
	bool had = false;
	template <class MakeMaps>
	agent_segment(const char *what, MakeMaps make_maps)
		: name(std::string("bpftime-") + what + "-" +
		       std::to_string(getpid()))
	{
		const char *was = getenv("BPFTIME_GLOBAL_SHM_NAME");
		had = was != nullptr;
		previous = was ? was : "";
		bpftime_destroy_global_shm();
		REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) ==
			0);
		bpftime_initialize_global_shm(
			shm_open_type::SHM_REMOVE_AND_CREATE);
		make_maps();
		bpftime_destroy_global_shm();
		bpftime_initialize_global_shm(shm_open_type::SHM_OPEN_ONLY);
	}
	~agent_segment()
	{
		bpftime_destroy_global_shm();
		shared_memory_object::remove(name.c_str());
		bpftime_initialize_global_shm(shm_open_type::SHM_NO_CREATE);
		if (had)
			setenv("BPFTIME_GLOBAL_SHM_NAME", previous.c_str(), 1);
		else
			unsetenv("BPFTIME_GLOBAL_SHM_NAME");
	}
};
} // namespace cuda_test

#endif

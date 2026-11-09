// GPU maps that export device memory through CUDA IPC (1501, 1502, 1503)
// cannot be created on GPUs without it: on the Orin cuIpcGetMemHandle returns
// CUDA_ERROR_NOT_SUPPORTED (results gpu/examples-20261109, problem 15). The
// syscall server creates them in host memory there.
#include <boost/interprocess/managed_shared_memory.hpp>
#include <bpftime_shm.hpp>
#include <bpftime_shm_internal.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <string>
#include <unistd.h>

using namespace boost::interprocess;
using namespace bpftime;

TEST_CASE("Map types that need CUDA IPC have host memory variants",
	  "[gpu-map-host]")
{
	using t = bpf_map_type;
	CHECK(gpu_map_type_without_cuda_ipc(
		      (int)t::BPF_MAP_TYPE_PERGPUTD_ARRAY_MAP) ==
	      (int)t::BPF_MAP_TYPE_PERGPUTD_ARRAY_HOST_MAP);
	CHECK(gpu_map_type_without_cuda_ipc((int)t::BPF_MAP_TYPE_GPU_ARRAY_MAP) ==
	      (int)t::BPF_MAP_TYPE_GPU_ARRAY_HOST_MAP);
	CHECK(gpu_map_type_without_cuda_ipc((int)t::BPF_MAP_TYPE_GPU_HASH_MAP) ==
	      (int)t::BPF_MAP_TYPE_HASH);
	// Already in host memory, or not GPU maps.
	for (int type : { (int)t::BPF_MAP_TYPE_GPU_RINGBUF_MAP,
			  (int)t::BPF_MAP_TYPE_PERGPUTD_ARRAY_HOST_MAP,
			  (int)t::BPF_MAP_TYPE_GPU_ARRAY_HOST_MAP,
			  (int)t::BPF_MAP_TYPE_HASH, (int)t::BPF_MAP_TYPE_ARRAY })
		CHECK(gpu_map_type_without_cuda_ipc(type) == type);
}

#if defined(BPFTIME_ENABLE_CUDA_ATTACH)
// GPU map creation turns the syscall server's mock off around the
// constructor; one that threw (cuIpcGetMemHandle 801, a non-numeric thread
// count) left it off, and every later bpf() of the tool went to the kernel.
TEST_CASE("A GPU map constructor that throws turns the mock back on",
	  "[gpu-map-host]")
{
	const std::string name =
		"bpftime-gpu-map-mock-" + std::to_string(getpid());
	const char *had = getenv("BPFTIME_GLOBAL_SHM_NAME");
	const std::string previous = had ? had : "";
	bpftime_destroy_global_shm();
	REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
	bpftime_initialize_global_shm(shm_open_type::SHM_REMOVE_AND_CREATE);
	bool mock = true;
	int switches = 0;
	shm_holder.global_shared_memory.set_mock_setter([&](bool on) {
		mock = on;
		switches++;
	});
	// Throws inside the constructor case on any GPU, IPC or not.
	REQUIRE(setenv("BPFTIME_MAP_GPU_THREAD_COUNT", "not-a-number", 1) == 0);
	CHECK(bpftime_maps_create(100, "gpu_map",
				  { .type = (int)bpf_map_type::
						    BPF_MAP_TYPE_PERGPUTD_ARRAY_MAP,
				    .key_size = 4,
				    .value_size = 8,
				    .max_ents = 4 }) < 0);
	unsetenv("BPFTIME_MAP_GPU_THREAD_COUNT");
	CHECK(switches >= 2);
	CHECK(mock);
	shm_holder.global_shared_memory.set_mock_setter([](bool) {});
	shm_holder.global_shared_memory.reset_server_state();
	bpftime_destroy_global_shm();
	shared_memory_object::remove(name.c_str());
	bpftime_initialize_global_shm(shm_open_type::SHM_NO_CREATE);
	if (had)
		setenv("BPFTIME_GLOBAL_SHM_NAME", previous.c_str(), 1);
	else
		unsetenv("BPFTIME_GLOBAL_SHM_NAME");
}
#endif

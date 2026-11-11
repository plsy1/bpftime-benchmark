// The GPU side of host calls, GPU ringbufs and per-thread maps
// (attach/nv_attach_impl/trampoline/default_trampoline.cu, shipped as
// trampoline_ptx.h) against the host side (the agent's CUDA watcher, the GPU
// ringbuf map). The GPU tests load the trampoline with small PTX kernels that
// call its helpers the way a probe does; they are skipped without a GPU.
#include "bpf_attach_ctx.hpp"
#include "bpf_map/gpu/nv_gpu_ringbuf_map.hpp"
#include "bpftime_shm.hpp"
#include "bpftime_shm_internal.hpp"
#include "catch2/catch_test_macros.hpp"
#include "cuda.h"
#include "nv_attach_impl.hpp"
#include "nv_attach_utils.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace bpftime;
using namespace boost::interprocess;

namespace
{
// Kernels appended to the trampoline. Each thread uses its global index
// (blockIdx.x * blockDim.x + threadIdx.x) as its key or record contents.
const char TEST_KERNELS_PTX[] = R"PTX(
// map update (key = i, value = 7 * i + 1), then map lookup of key i, by every
// thread, `rounds` times: out[2 i] = update result, out[2 i + 1] = the value
// found (or -1)
.visible .entry bpftime_test_host_calls(
	.param .u64 bpftime_test_host_calls_param_0,
	.param .u64 bpftime_test_host_calls_param_1,
	.param .u64 bpftime_test_host_calls_param_2
)
{
	.local .align 8 .b8 	__local_depot_hc[16];
	.reg .b64 	%SP;
	.reg .b64 	%SPL;
	.reg .pred 	%p<3>;
	.reg .b32 	%r<5>;
	.reg .b64 	%rd<20>;

	mov.u64 	%SPL, __local_depot_hc;
	cvta.local.u64 	%SP, %SPL;
	ld.param.u64 	%rd1, [bpftime_test_host_calls_param_0];
	ld.param.u64 	%rd2, [bpftime_test_host_calls_param_1];
	ld.param.u64 	%rd15, [bpftime_test_host_calls_param_2];
	mov.u64 	%rd16, 0;
$L_hc_again:
	mov.u32 	%r1, %ctaid.x;
	mov.u32 	%r2, %ntid.x;
	mov.u32 	%r3, %tid.x;
	mad.lo.s32 	%r4, %r1, %r2, %r3;
	cvt.u64.u32 	%rd3, %r4;
	mul.lo.s64 	%rd4, %rd3, 7;
	add.s64 	%rd4, %rd4, 1;
	st.local.u64 	[%SPL], %rd3;
	st.local.u64 	[%SPL+8], %rd4;
	add.s64 	%rd5, %SP, 8;
	mov.u64 	%rd6, 0;
	{
	.param .b64 param0;
	st.param.b64 	[param0+0], %rd1;
	.param .b64 param1;
	st.param.b64 	[param1+0], %SP;
	.param .b64 param2;
	st.param.b64 	[param2+0], %rd5;
	.param .b64 param3;
	st.param.b64 	[param3+0], %rd6;
	.param .b64 param4;
	st.param.b64 	[param4+0], %rd6;
	.param .b64 retval0;
	call.uni (retval0), _bpf_helper_ext_0002, (param0, param1, param2, param3, param4);
	ld.param.b64 	%rd7, [retval0+0];
	}
	{
	.param .b64 param0;
	st.param.b64 	[param0+0], %rd1;
	.param .b64 param1;
	st.param.b64 	[param1+0], %SP;
	.param .b64 param2;
	st.param.b64 	[param2+0], %rd6;
	.param .b64 param3;
	st.param.b64 	[param3+0], %rd6;
	.param .b64 param4;
	st.param.b64 	[param4+0], %rd6;
	.param .b64 retval0;
	call.uni (retval0), _bpf_helper_ext_0001, (param0, param1, param2, param3, param4);
	ld.param.b64 	%rd8, [retval0+0];
	}
	mov.b64 	%rd9, -1;
	setp.eq.s64 	%p1, %rd8, 0;
	@%p1 bra 	$L_hc_store;
	ld.u64 	%rd9, [%rd8];
$L_hc_store:
	shl.b64 	%rd10, %rd3, 4;
	add.s64 	%rd11, %rd2, %rd10;
	st.u64 	[%rd11], %rd7;
	st.u64 	[%rd11+8], %rd9;
	add.s64 	%rd16, %rd16, 1;
	setp.lt.u64 	%p2, %rd16, %rd15;
	@%p2 bra 	$L_hc_again;
	ret;
}

// `count` records { i, sequence number, 0xbeef, 0xbeef } of `size` bytes into
// a GPU ringbuf: out[i] = how many of them were written
.visible .entry bpftime_test_ringbuf(
	.param .u64 bpftime_test_ringbuf_param_0,
	.param .u64 bpftime_test_ringbuf_param_1,
	.param .u64 bpftime_test_ringbuf_param_2,
	.param .u64 bpftime_test_ringbuf_param_3
)
{
	.local .align 8 .b8 	__local_depot_rb[32];
	.reg .b64 	%SP;
	.reg .b64 	%SPL;
	.reg .pred 	%p<3>;
	.reg .b32 	%r<5>;
	.reg .b64 	%rd<20>;

	mov.u64 	%SPL, __local_depot_rb;
	cvta.local.u64 	%SP, %SPL;
	ld.param.u64 	%rd1, [bpftime_test_ringbuf_param_0];
	ld.param.u64 	%rd2, [bpftime_test_ringbuf_param_1];
	ld.param.u64 	%rd3, [bpftime_test_ringbuf_param_2];
	ld.param.u64 	%rd4, [bpftime_test_ringbuf_param_3];
	mov.u32 	%r1, %ctaid.x;
	mov.u32 	%r2, %ntid.x;
	mov.u32 	%r3, %tid.x;
	mad.lo.s32 	%r4, %r1, %r2, %r3;
	cvt.u64.u32 	%rd5, %r4;
	mov.u64 	%rd6, 0;
	mov.u64 	%rd7, 0;
	mov.u64 	%rd8, 0;
	mov.u64 	%rd12, 48879;
$L_rb_loop:
	setp.ge.u64 	%p1, %rd6, %rd2;
	@%p1 bra 	$L_rb_done;
	st.local.u64 	[%SPL], %rd5;
	st.local.u64 	[%SPL+8], %rd6;
	st.local.u64 	[%SPL+16], %rd12;
	st.local.u64 	[%SPL+24], %rd12;
	{
	.param .b64 param0;
	st.param.b64 	[param0+0], %rd8;
	.param .b64 param1;
	st.param.b64 	[param1+0], %rd1;
	.param .b64 param2;
	st.param.b64 	[param2+0], %rd8;
	.param .b64 param3;
	st.param.b64 	[param3+0], %SP;
	.param .b64 param4;
	st.param.b64 	[param4+0], %rd3;
	.param .b64 retval0;
	call.uni (retval0), _bpf_helper_ext_0025, (param0, param1, param2, param3, param4);
	ld.param.b64 	%rd9, [retval0+0];
	}
	setp.eq.s64 	%p2, %rd9, 0;
	selp.u64 	%rd10, 1, 0, %p2;
	add.s64 	%rd7, %rd7, %rd10;
	add.s64 	%rd6, %rd6, 1;
	bra.uni 	$L_rb_loop;
$L_rb_done:
	shl.b64 	%rd11, %rd5, 3;
	add.s64 	%rd13, %rd4, %rd11;
	st.u64 	[%rd13], %rd7;
	ret;
}

// map update (key, value = i + 100), then map lookup of key, by every thread
// in a per-thread array map: out[2 i] = update result, out[2 i + 1] = pointer
.visible .entry bpftime_test_thread_map(
	.param .u64 bpftime_test_thread_map_param_0,
	.param .u64 bpftime_test_thread_map_param_1,
	.param .u64 bpftime_test_thread_map_param_2
)
{
	.local .align 8 .b8 	__local_depot_tm[16];
	.reg .b64 	%SP;
	.reg .b64 	%SPL;
	.reg .b32 	%r<5>;
	.reg .b64 	%rd<16>;

	mov.u64 	%SPL, __local_depot_tm;
	cvta.local.u64 	%SP, %SPL;
	ld.param.u64 	%rd1, [bpftime_test_thread_map_param_0];
	ld.param.u64 	%rd2, [bpftime_test_thread_map_param_1];
	ld.param.u64 	%rd3, [bpftime_test_thread_map_param_2];
	mov.u32 	%r1, %ctaid.x;
	mov.u32 	%r2, %ntid.x;
	mov.u32 	%r3, %tid.x;
	mad.lo.s32 	%r4, %r1, %r2, %r3;
	cvt.u64.u32 	%rd5, %r4;
	add.s64 	%rd6, %rd5, 100;
	st.local.u64 	[%SPL], %rd2;
	st.local.u64 	[%SPL+8], %rd6;
	add.s64 	%rd7, %SP, 8;
	mov.u64 	%rd8, 0;
	{
	.param .b64 param0;
	st.param.b64 	[param0+0], %rd1;
	.param .b64 param1;
	st.param.b64 	[param1+0], %SP;
	.param .b64 param2;
	st.param.b64 	[param2+0], %rd7;
	.param .b64 param3;
	st.param.b64 	[param3+0], %rd8;
	.param .b64 param4;
	st.param.b64 	[param4+0], %rd8;
	.param .b64 retval0;
	call.uni (retval0), _bpf_helper_ext_0002, (param0, param1, param2, param3, param4);
	ld.param.b64 	%rd9, [retval0+0];
	}
	{
	.param .b64 param0;
	st.param.b64 	[param0+0], %rd1;
	.param .b64 param1;
	st.param.b64 	[param1+0], %SP;
	.param .b64 param2;
	st.param.b64 	[param2+0], %rd8;
	.param .b64 param3;
	st.param.b64 	[param3+0], %rd8;
	.param .b64 param4;
	st.param.b64 	[param4+0], %rd8;
	.param .b64 retval0;
	call.uni (retval0), _bpf_helper_ext_0001, (param0, param1, param2, param3, param4);
	ld.param.b64 	%rd10, [retval0+0];
	}
	shl.b64 	%rd11, %rd5, 4;
	add.s64 	%rd12, %rd3, %rd11;
	st.u64 	[%rd12], %rd9;
	st.u64 	[%rd12+8], %rd10;
	ret;
}

// One thread: out[0] = update(map, &key, &42, flags), out[1] = delete(map,
// &key), out[2] = the value lookup(map, &key) finds (-1 for NULL)
.visible .entry bpftime_test_map_edges(
	.param .u64 bpftime_test_map_edges_param_0,
	.param .u64 bpftime_test_map_edges_param_1,
	.param .u64 bpftime_test_map_edges_param_2,
	.param .u64 bpftime_test_map_edges_param_3
)
{
	.local .align 8 .b8 	__local_depot_me[16];
	.reg .b64 	%SP;
	.reg .b64 	%SPL;
	.reg .pred 	%p<2>;
	.reg .b64 	%rd<16>;

	mov.u64 	%SPL, __local_depot_me;
	cvta.local.u64 	%SP, %SPL;
	ld.param.u64 	%rd1, [bpftime_test_map_edges_param_0];
	ld.param.u64 	%rd2, [bpftime_test_map_edges_param_1];
	ld.param.u64 	%rd3, [bpftime_test_map_edges_param_2];
	ld.param.u64 	%rd4, [bpftime_test_map_edges_param_3];
	mov.u64 	%rd5, 42;
	st.local.u64 	[%SPL], %rd2;
	st.local.u64 	[%SPL+8], %rd5;
	add.s64 	%rd6, %SP, 8;
	mov.u64 	%rd7, 0;
	{
	.param .b64 param0;
	st.param.b64 	[param0+0], %rd1;
	.param .b64 param1;
	st.param.b64 	[param1+0], %SP;
	.param .b64 param2;
	st.param.b64 	[param2+0], %rd6;
	.param .b64 param3;
	st.param.b64 	[param3+0], %rd3;
	.param .b64 param4;
	st.param.b64 	[param4+0], %rd7;
	.param .b64 retval0;
	call.uni (retval0), _bpf_helper_ext_0002, (param0, param1, param2, param3, param4);
	ld.param.b64 	%rd8, [retval0+0];
	}
	{
	.param .b64 param0;
	st.param.b64 	[param0+0], %rd1;
	.param .b64 param1;
	st.param.b64 	[param1+0], %SP;
	.param .b64 param2;
	st.param.b64 	[param2+0], %rd7;
	.param .b64 param3;
	st.param.b64 	[param3+0], %rd7;
	.param .b64 param4;
	st.param.b64 	[param4+0], %rd7;
	.param .b64 retval0;
	call.uni (retval0), _bpf_helper_ext_0003, (param0, param1, param2, param3, param4);
	ld.param.b64 	%rd9, [retval0+0];
	}
	{
	.param .b64 param0;
	st.param.b64 	[param0+0], %rd1;
	.param .b64 param1;
	st.param.b64 	[param1+0], %SP;
	.param .b64 param2;
	st.param.b64 	[param2+0], %rd7;
	.param .b64 param3;
	st.param.b64 	[param3+0], %rd7;
	.param .b64 param4;
	st.param.b64 	[param4+0], %rd7;
	.param .b64 retval0;
	call.uni (retval0), _bpf_helper_ext_0001, (param0, param1, param2, param3, param4);
	ld.param.b64 	%rd10, [retval0+0];
	}
	mov.b64 	%rd11, -1;
	setp.eq.s64 	%p1, %rd10, 0;
	@%p1 bra 	$L_me_store;
	ld.u64 	%rd11, [%rd10];
$L_me_store:
	st.u64 	[%rd4], %rd8;
	st.u64 	[%rd4+8], %rd9;
	st.u64 	[%rd4+16], %rd11;
	ret;
}

// out[i] = bpf_get_current_pid_tgid() of thread i
.visible .entry bpftime_test_pid_tgid(
	.param .u64 bpftime_test_pid_tgid_param_0
)
{
	.reg .b32 	%r<5>;
	.reg .b64 	%rd<8>;

	ld.param.u64 	%rd1, [bpftime_test_pid_tgid_param_0];
	mov.u64 	%rd2, 0;
	{
	.param .b64 param0;
	st.param.b64 	[param0+0], %rd2;
	.param .b64 param1;
	st.param.b64 	[param1+0], %rd2;
	.param .b64 param2;
	st.param.b64 	[param2+0], %rd2;
	.param .b64 param3;
	st.param.b64 	[param3+0], %rd2;
	.param .b64 param4;
	st.param.b64 	[param4+0], %rd2;
	.param .b64 retval0;
	call.uni (retval0), _bpf_helper_ext_0014, (param0, param1, param2, param3, param4);
	ld.param.b64 	%rd3, [retval0+0];
	}
	mov.u32 	%r1, %ctaid.x;
	mov.u32 	%r2, %ntid.x;
	mov.u32 	%r3, %tid.x;
	mad.lo.s32 	%r4, %r1, %r2, %r3;
	cvt.u64.u32 	%rd4, %r4;
	shl.b64 	%rd5, %rd4, 3;
	add.s64 	%rd6, %rd1, %rd5;
	st.u64 	[%rd6], %rd3;
	ret;
}
)PTX";

constexpr uint64_t E2BIG_RESULT = (uint64_t)-7;

// The primary context of device 0, current on this thread; false without a
// usable GPU.
bool use_primary_context()
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

std::string device_sm_arch()
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

// The trampoline and the test kernels in one module, as bpftime builds a
// probed module.
struct test_module {
	CUmodule module = nullptr;

	test_module()
	{
		const auto ptx = attach::wrap_ptx_with_trampoline_for_sm(
			TEST_KERNELS_PTX, device_sm_arch());
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

ringbuf_header *thread_header(nv_gpu_ringbuf_map_impl *rb, uint64_t thread)
{
	return (ringbuf_header *)(rb->host_data_buffer() +
				  thread * rb->thread_records_size());
}

// Writes one record the way the GPU does: the record, then tail.
void publish(nv_gpu_ringbuf_map_impl *rb, uint64_t thread, uint64_t entries,
	     uint64_t value_size, uint64_t size, uint64_t first_word)
{
	auto header = thread_header(rb, thread);
	char *record = (char *)header + sizeof(ringbuf_header) +
		       (header->tail % entries) * (sizeof(uint64_t) + value_size);
	memcpy(record, &size, sizeof(size));
	memcpy(record + sizeof(uint64_t), &first_word, sizeof(first_word));
	__atomic_store_n(&header->tail, header->tail + 1, __ATOMIC_RELEASE);
}

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

using records_by_thread = std::map<uint64_t, std::vector<uint64_t>>;

// Drains rb; the records' first two words are a thread index and a
// sequence number.
int drain(nv_gpu_ringbuf_map_impl *rb, records_by_thread &records,
	  std::vector<uint64_t> *sizes = nullptr)
{
	return rb->drain_data([&](const void *data, uint64_t size) {
		uint64_t words[2] = {};
		memcpy(words, data, std::min<uint64_t>(size, sizeof(words)));
		records[words[0]].push_back(words[1]);
		if (sizes)
			sizes->push_back(size);
	});
}
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
} // namespace

// The host used to take one record per GPU thread per poll, return 0, and
// stop at the first thread whose `dirty` flag was set; head moved before the
// record was read.
TEST_CASE("A GPU ringbuf poll passes every published record once",
	  "[cuda-ringbuf]")
{
	scratch_segment scratch("gpu-ringbuf-drain", 1 << 20);
	constexpr uint64_t value_size = 16, entries = 4, threads = 3;
	auto rb = scratch.segment.construct<nv_gpu_ringbuf_map_impl>(
		anonymous_instance)(scratch.segment, value_size, entries,
				    threads);

	// thread 0: 3 records; thread 1 is in the middle of writing (busy set,
	// tail not yet moved); thread 2: full
	for (uint64_t seq = 0; seq < 3; seq++)
		publish(rb, 0, entries, value_size, value_size, 0 + seq);
	thread_header(rb, 1)->busy = 1;
	for (uint64_t seq = 0; seq < 4; seq++)
		publish(rb, 2, entries, value_size, value_size, 20 + seq);

	std::map<uint64_t, std::vector<uint64_t>> by_first_word;
	auto collect = [&](const void *data, uint64_t size) {
		uint64_t word;
		memcpy(&word, data, sizeof(word));
		by_first_word[word / 10].push_back(word % 10);
		CHECK(size == value_size);
	};
	REQUIRE(rb->drain_data(collect) == 7);
	CHECK(by_first_word[0] == std::vector<uint64_t>{ 0, 1, 2 });
	CHECK(by_first_word[2] == std::vector<uint64_t>{ 0, 1, 2, 3 });
	for (uint64_t t = 0; t < threads; t++)
		CHECK(thread_header(rb, t)->head == thread_header(rb, t)->tail);
	REQUIRE(rb->drain_data(collect) == 0);

	// around the end of the records
	by_first_word.clear();
	for (uint64_t seq = 3; seq < 6; seq++)
		publish(rb, 0, entries, value_size, value_size, 0 + seq);
	REQUIRE(rb->drain_data(collect) == 3);
	CHECK(by_first_word[0] == std::vector<uint64_t>{ 3, 4, 5 });

	// a size the map cannot hold reaches the callback as value_size
	publish(rb, 1, entries, value_size, 1 << 20, 10);
	std::vector<uint64_t> sizes;
	REQUIRE(rb->drain_data([&](const void *, uint64_t size) {
		sizes.push_back(size);
	}) == 1);
	CHECK(sizes == std::vector<uint64_t>{ value_size });
}

// GPU threads used to write their key, value or printk arguments into the one
// request slot before taking the lock, all at once, and every request carried
// whatever the last writer left there (results gpu/examples-20261109,
// problem 2). The watcher slept 1 ms after every request (problem 10).
TEST_CASE("Every GPU thread's host call carries its own arguments",
	  "[cuda-host-call]")
{
	if (!use_primary_context())
		SKIP("no CUDA device");
	const std::string name =
		"bpftime-cuda-host-call-" + std::to_string(getpid());
	const char *had = getenv("BPFTIME_GLOBAL_SHM_NAME");
	const std::string previous = had ? had : "";
	bpftime_destroy_global_shm();
	REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);

	// the syscall server creates the map, the agent opens the segment
	bpftime_initialize_global_shm(shm_open_type::SHM_REMOVE_AND_CREATE);
	constexpr int map_fd = 7;
	REQUIRE(bpftime_maps_create(
			map_fd, "host_call_map",
			{ .type = (int)bpf_map_type::BPF_MAP_TYPE_HASH,
			  .key_size = 8,
			  .value_size = 8,
			  .max_ents = 1024 }) == map_fd);
	bpftime_destroy_global_shm();
	bpftime_initialize_global_shm(shm_open_type::SHM_OPEN_ONLY);
	{
		bpf_attach_ctx ctx;
		const uintptr_t comm =
			cuda::cuda_shared_mem_device_pointer_storage().load();
		REQUIRE(comm != 0);

		test_module module;
		module.set_comm(comm);
		module.set_map_info(map_fd, { .enabled = true,
					      .key_size = 8,
					      .value_size = 8,
					      .max_entries = 1024,
					      .map_type = (int)bpf_map_type::
						      BPF_MAP_TYPE_HASH,
					      .extra_buffer = nullptr,
					      .max_thread_count = 0 });
		// 32 warps, all waiting for the one request slot
		constexpr unsigned blocks = 8, threads = 128;
		constexpr uint64_t n = blocks * threads;
		device_buffer out(n * 2 * sizeof(uint64_t));
		// The first launch also loads the module's code (lazy loading).
		module.launch("bpftime_test_host_calls", blocks, threads,
			      { map_fd, out.ptr, 1 });
		const auto start = std::chrono::steady_clock::now();
		module.launch("bpftime_test_host_calls", blocks, threads,
			      { map_fd, out.ptr, 1 });
		const auto elapsed =
			std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - start)
				.count();
		const auto results = out.read();
		uint64_t right = 0;
		for (uint64_t i = 0; i < n; i++) {
			const uint64_t expected = 7 * i + 1;
			auto stored = (const uint64_t *)bpftime_map_lookup_elem(
				map_fd, &i);
			if (results[2 * i] == 0 && results[2 * i + 1] == expected &&
			    stored && *stored == expected)
				right++;
			else
				UNSCOPED_INFO("thread "
					      << i << ": update "
					      << (int64_t)results[2 * i]
					      << ", lookup " << results[2 * i + 1]
					      << ", map "
					      << (stored ? *stored : 0));
		}
		CHECK(right == n);
		// 2048 calls, about 20 ms. A sleep of the watcher per call took
		// 2.2 s; waiting on the lock in host memory, 616 ms.
		INFO(2 * n << " host calls in " << elapsed << " ms");
		CHECK(elapsed < 200);
	}
	bpftime_destroy_global_shm();
	shared_memory_object::remove(name.c_str());
	bpftime_initialize_global_shm(shm_open_type::SHM_NO_CREATE);
	if (had)
		setenv("BPFTIME_GLOBAL_SHM_NAME", previous.c_str(), 1);
	else
		unsetenv("BPFTIME_GLOBAL_SHM_NAME");
}

// The GPU used to publish tail before writing the record, write records
// larger than value_size over the next one, and count nothing it dropped.
TEST_CASE("GPU threads write whole ringbuf records and count the dropped ones",
	  "[cuda-ringbuf]")
{
	if (!use_primary_context())
		SKIP("no CUDA device");
	scratch_segment scratch("gpu-ringbuf-device", 4 << 20);
	constexpr uint64_t value_size = 16, entries = 4;
	constexpr unsigned blocks = 2, threads = 32;
	constexpr uint64_t n = blocks * threads;
	auto rb = scratch.segment.construct<nv_gpu_ringbuf_map_impl>(
		anonymous_instance)(scratch.segment, value_size, entries, n);
	REQUIRE(cuMemHostRegister(scratch.segment.get_address(),
				  scratch.segment.get_size(),
				  CU_MEMHOSTREGISTER_DEVICEMAP |
					  CU_MEMHOSTREGISTER_PORTABLE) ==
		CUDA_SUCCESS);
	CUdeviceptr records_on_device;
	REQUIRE(cuMemHostGetDevicePointer(&records_on_device,
					  rb->host_data_buffer(),
					  0) == CUDA_SUCCESS);
	{
		test_module module;
		constexpr int map_fd = 5;
		module.set_map_info(
			map_fd,
			{ .enabled = true,
			  .key_size = 0,
			  .value_size = (int)value_size,
			  .max_entries = (int)entries,
			  .map_type = (int)bpf_map_type::
				  BPF_MAP_TYPE_GPU_RINGBUF_MAP,
			  .extra_buffer = (void *)records_on_device,
			  .max_thread_count = n });
		device_buffer written(n * sizeof(uint64_t));
		auto dropped = [&](uint64_t thread) {
			return __atomic_load_n(
				&thread_header(rb, thread)->dropped,
				__ATOMIC_RELAXED);
		};

		// 6 records per thread, room for 4
		module.launch("bpftime_test_ringbuf", blocks, threads,
			      { map_fd, 6, value_size, written.ptr });
		auto counts = written.read();
		records_by_thread records;
		std::vector<uint64_t> sizes;
		REQUIRE(drain(rb, records, &sizes) == (int)(n * entries));
		for (uint64_t i = 0; i < n; i++) {
			INFO("thread " << i);
			CHECK(counts[i] == entries);
			CHECK(records[i] ==
			      std::vector<uint64_t>{ 0, 1, 2, 3 });
			CHECK(dropped(i) == 2);
		}
		for (auto size : sizes)
			CHECK(size == value_size);

		// records larger than value_size are refused
		module.launch("bpftime_test_ringbuf", blocks, threads,
			      { map_fd, 3, value_size + 8, written.ptr });
		counts = written.read();
		records.clear();
		REQUIRE(drain(rb, records) == 0);
		for (uint64_t i = 0; i < n; i++) {
			INFO("thread " << i);
			CHECK(counts[i] == 0);
			CHECK(dropped(i) == 5);
		}

		// the places read by the poll are free again
		module.launch("bpftime_test_ringbuf", blocks, threads,
			      { map_fd, 4, value_size, written.ptr });
		REQUIRE(drain(rb, records) == (int)(n * entries));
		for (uint64_t i = 0; i < n; i++)
			CHECK(records[i] == std::vector<uint64_t>{ 0, 1, 2, 3 });
	}
	cuMemHostUnregister(scratch.segment.get_address());
}

// Threads beyond the per-thread map's thread count, and keys beyond its
// entries, used to read and write past the thread's slot: into other entries
// and past the map (threadhist corrupted the shared segment this way).
TEST_CASE("Per-thread GPU maps refuse threads and keys outside them",
	  "[cuda-host-call]")
{
	if (!use_primary_context())
		SKIP("no CUDA device");
	constexpr uint64_t map_threads = 64, entries = 2, value_size = 8;
	constexpr size_t map_bytes = map_threads * entries * value_size;
	constexpr size_t buffer_bytes = 2 * map_bytes; // a guard after the map
	void *host = nullptr;
	REQUIRE(cuMemHostAlloc(&host, buffer_bytes,
			       CU_MEMHOSTALLOC_DEVICEMAP) == CUDA_SUCCESS);
	memset(host, 0xaa, buffer_bytes);
	CUdeviceptr on_device;
	REQUIRE(cuMemHostGetDevicePointer(&on_device, host, 0) ==
		CUDA_SUCCESS);
	auto untouched = [&](size_t from) {
		for (size_t i = from; i < buffer_bytes; i++)
			if (((unsigned char *)host)[i] != 0xaa)
				return false;
		return true;
	};
	{
		test_module module;
		constexpr int map_fd = 6;
		module.set_map_info(
			map_fd,
			{ .enabled = true,
			  .key_size = 4,
			  .value_size = (int)value_size,
			  .max_entries = (int)entries,
			  .map_type = (int)bpf_map_type::
				  BPF_MAP_TYPE_PERGPUTD_ARRAY_HOST_MAP,
			  .extra_buffer = (void *)on_device,
			  .max_thread_count = map_threads });
		constexpr unsigned blocks = 2, threads = 64;
		constexpr uint64_t n = blocks * threads;
		device_buffer out(n * 2 * sizeof(uint64_t));

		module.launch("bpftime_test_thread_map", blocks, threads,
			      { map_fd, 0, out.ptr });
		auto results = out.read();
		for (uint64_t i = 0; i < n; i++) {
			INFO("thread " << i);
			if (i < map_threads) {
				CHECK(results[2 * i] == 0);
				CHECK(results[2 * i + 1] ==
				      on_device + i * value_size);
				CHECK(((uint64_t *)host)[i] == i + 100);
			} else {
				CHECK(results[2 * i] == E2BIG_RESULT);
				CHECK(results[2 * i + 1] == 0);
			}
		}
		// entry 1 and the guard
		CHECK(untouched(map_threads * value_size));

		module.launch("bpftime_test_thread_map", blocks, threads,
			      { map_fd, entries, out.ptr });
		results = out.read();
		for (uint64_t i = 0; i < n; i++) {
			INFO("thread " << i);
			CHECK(results[2 * i] == E2BIG_RESULT);
			CHECK(results[2 * i + 1] == 0);
		}
		CHECK(untouched(map_threads * value_size));
	}
	cuMemFreeHost(host);
}

// The watcher turned a map value's host address into "CommSharedMem's
// device address + the distance from CommSharedMem": an illegal address for
// memory outside the registered segment, which the GPU thread then used.
TEST_CASE("Map values reach the GPU only through memory it can address",
	  "[cuda-host-call]")
{
	if (!use_primary_context())
		SKIP("no CUDA device");
	void *registered = nullptr;
	REQUIRE(cuMemHostAlloc(&registered, 1 << 16,
			       CU_MEMHOSTALLOC_DEVICEMAP) == CUDA_SUCCESS);
	CUdeviceptr mapped;
	REQUIRE(cuMemHostGetDevicePointer(&mapped, registered, 0) ==
		CUDA_SUCCESS);
	CHECK(cuda::gpu_address_of_map_value((char *)registered + 100) ==
	      (void *)(mapped + 100));

	auto unregistered = std::make_unique<uint64_t[]>(1024);
	CHECK(cuda::gpu_address_of_map_value(unregistered.get() + 3) ==
	      nullptr);

	CUdeviceptr device;
	REQUIRE(cuMemAlloc(&device, 4096) == CUDA_SUCCESS);
	CHECK(cuda::gpu_address_of_map_value((void *)(device + 8)) ==
	      (void *)(device + 8));
	cuMemFree(device);
	cuMemFreeHost(registered);
}

// An ordinary array map's values are in the segment, which the agent
// registers with CUDA. Every lookup, update and delete of one used to be a
// host call (about 10 us each, one at a time; results gpu/compare-20261110:
// CPU Array map update 324 us for 32 calls). The agent now gives the
// trampoline their device address and the GPU follows array_map_impl's rules.
TEST_CASE("The GPU reads and writes ordinary array maps without host calls",
	  "[cuda-host-call]")
{
	if (!use_primary_context())
		SKIP("no CUDA device");
	constexpr int map_fd = 7;
	constexpr uint64_t entries = 1024;
	agent_segment segment("cuda-array-map", [&]() {
		REQUIRE(bpftime_maps_create(
				map_fd, "array_map",
				{ .type = (int)bpf_map_type::BPF_MAP_TYPE_ARRAY,
				  .key_size = 4,
				  .value_size = 8,
				  .max_ents = entries }) == map_fd);
	});
	{
		bpf_attach_ctx ctx;
		const auto infos = ctx.create_map_basic_info(16);
		REQUIRE(infos.size() == 16);
		const auto &info = infos[map_fd];
		REQUIRE(info.enabled);
		REQUIRE(info.map_type ==
			(int)bpf_map_type::BPF_MAP_TYPE_ARRAY);
		// the device address of the values
		REQUIRE(info.extra_buffer != nullptr);
		// a request slot nobody but the test serves: any host call
		// shows in `served`
		counting_host host;
		test_module module;
		module.set_comm(host.comm_on_device);
		module.set_map_info(map_fd, info);

		// every thread updates its key, then looks it up
		constexpr unsigned blocks = 8, threads = 128;
		constexpr uint64_t n = blocks * threads;
		device_buffer out(n * 2 * sizeof(uint64_t));
		module.launch("bpftime_test_host_calls", blocks, threads,
			      { map_fd, out.ptr, 1 });
		const auto results = out.read();
		uint64_t right = 0;
		for (uint32_t i = 0; i < n; i++) {
			const uint64_t expected = 7 * (uint64_t)i + 1;
			auto stored = (const uint64_t *)bpftime_map_lookup_elem(
				map_fd, &i);
			if (results[2 * i] == 0 && results[2 * i + 1] == expected &&
			    stored && *stored == expected)
				right++;
			else
				UNSCOPED_INFO("thread "
					      << i << ": update "
					      << (int64_t)results[2 * i]
					      << ", lookup " << results[2 * i + 1]
					      << ", map "
					      << (stored ? *stored : 0));
		}
		CHECK(right == n);

		// update (42) with flags, delete, lookup of one key
		device_buffer edge(3 * sizeof(uint64_t));
		auto run = [&](uint64_t key, uint64_t flags) {
			module.launch("bpftime_test_map_edges", 1, 1,
				      { map_fd, key, flags, edge.ptr });
			return edge.read();
		};
		// BPF_NOEXIST: every element exists; elements cannot be deleted
		auto r = run(3, 1);
		CHECK((int64_t)r[0] == -1);
		CHECK((int64_t)r[1] == -1);
		CHECK(r[2] == 7 * 3 + 1);
		// BPF_EXIST
		r = run(3, 2);
		CHECK(r[0] == 0);
		CHECK(r[2] == 42);
		// no flag past BPF_EXIST in the low 32 bits
		r = run(4, 3);
		CHECK((int64_t)r[0] == -1);
		CHECK(r[2] == 7 * 4 + 1);
		// bpftime's own operations in the high 32 bits pass
		r = run(5, 1ULL << 32);
		CHECK(r[0] == 0);
		CHECK(r[2] == 42);
		// BPF_NOEXIST with bpftime's high bits: array_map_impl compares
		// the whole flags with BPF_NOEXIST, so the update goes through
		r = run(6, (1ULL << 32) | 1);
		CHECK(r[0] == 0);
		CHECK(r[2] == 42);
		// a key past max_entries: no update, lookup NULL
		r = run(entries, 0);
		CHECK((int64_t)r[0] == -1);
		CHECK((int64_t)r[2] == -1);

		CHECK(host.served.load() == 0);
	}
}

// bpf_get_current_pid_tgid was a host call for every GPU thread, and its low
// half was the watcher thread's tid. The agent now writes the process's tgid
// (in both halves) into every module it loads; GPU programs read the high
// half (upstream's cuda-counter, cudagraph, launchlate).
TEST_CASE("GPU threads read bpf_get_current_pid_tgid from their module",
	  "[cuda-host-call]")
{
	if (!use_primary_context())
		SKIP("no CUDA device");
	const uint64_t tgid = (uint32_t)getpid();
	const uint64_t expected = (tgid << 32) | tgid;
	CHECK(attach::trampoline_pid_tgid() == expected);
	counting_host host;
	{
		test_module module;
		module.set_comm(host.comm_on_device);
		REQUIRE(attach::write_trampoline_pid_tgid(module.module));
		constexpr unsigned blocks = 2, threads = 64;
		constexpr uint64_t n = blocks * threads;
		device_buffer out(n * sizeof(uint64_t));
		module.launch("bpftime_test_pid_tgid", blocks, threads,
			      { out.ptr });
		const auto results = out.read();
		uint64_t right = 0;
		for (uint64_t i = 0; i < n; i++)
			right += results[i] == expected;
		CHECK(right == n);
		CHECK(host.served.load() == 0);
	}
	{
		// not written: the helper asks the host
		test_module module;
		module.set_comm(host.comm_on_device);
		device_buffer out(sizeof(uint64_t));
		module.launch("bpftime_test_pid_tgid", 1, 1, { out.ptr });
		CHECK(host.served.load() == 1);
	}
}

TEST_CASE("The host answers bpf_get_current_pid_tgid with the tgid in both halves",
	  "[cuda-host-call]")
{
	if (!use_primary_context())
		SKIP("no CUDA device");
	agent_segment segment("cuda-pid-tgid", []() {});
	{
		bpf_attach_ctx ctx;
		const uintptr_t comm =
			cuda::cuda_shared_mem_device_pointer_storage().load();
		REQUIRE(comm != 0);
		test_module module; // __bpftime_pid_tgid not written
		module.set_comm(comm);
		device_buffer out(sizeof(uint64_t));
		module.launch("bpftime_test_pid_tgid", 1, 1, { out.ptr });
		const uint64_t tgid = (uint32_t)getpid();
		CHECK(out.read()[0] == ((tgid << 32) | tgid));
	}
}

// Patched modules keep the array's device address for good (they cannot be
// unloaded, and a loader that exits without a detach leaves their probes
// running). Unpinned, the storage went back to the segment when the loader
// closed the map, and GPU updates wrote into whatever was allocated there
// next. The agent pins the map for its context's life.
TEST_CASE("An array map the GPU addresses stays allocated after its fd is closed",
	  "[cuda-host-call]")
{
	if (!use_primary_context())
		SKIP("no CUDA device");
	constexpr int map_fd = 7, next_fd = 8;
	const bpftime::bpf_map_attr attr = {
		.type = (int)bpf_map_type::BPF_MAP_TYPE_ARRAY,
		.key_size = 4,
		.value_size = 8,
		.max_ents = 1024
	};
	agent_segment segment("cuda-array-pin", [&]() {
		REQUIRE(bpftime_maps_create(map_fd, "array_map", attr) ==
			map_fd);
	});
	{
		bpf_attach_ctx ctx;
		const auto info = ctx.create_map_basic_info(16)[map_fd];
		REQUIRE(info.extra_buffer != nullptr);
		// the loader closes the map and makes another of the same size
		bpftime_close(map_fd);
		REQUIRE(bpftime_maps_create(next_fd, "array_map_2", attr) ==
			next_fd);
		counting_host host;
		test_module module;
		module.set_comm(host.comm_on_device);
		module.set_map_info(map_fd, info);
		constexpr unsigned blocks = 8, threads = 128;
		constexpr uint64_t n = blocks * threads;
		device_buffer out(n * 2 * sizeof(uint64_t));
		module.launch("bpftime_test_host_calls", blocks, threads,
			      { map_fd, out.ptr, 1 });
		// the GPU wrote into the pinned old storage, not the new map
		uint64_t touched = 0;
		for (uint32_t i = 0; i < n; i++) {
			auto value = (const uint64_t *)bpftime_map_lookup_elem(
				next_fd, &i);
			if (!value || *value != 0)
				touched++;
		}
		CHECK(touched == 0);
		CHECK(host.served.load() == 0);
	}
}

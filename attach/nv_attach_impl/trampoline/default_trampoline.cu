#ifdef BPFTIME_TRAMPOLINE_DEVICE_ONLY
// Device code only, without CUDA's or the host's headers:
//   clang++ -x cuda --cuda-device-only -nocudainc -nocudalib
//     --cuda-gpu-arch=sm_61 -O2 -S -DBPFTIME_TRAMPOLINE_DEVICE_ONLY
// clang 15/16 cannot parse CUDA 13's headers, nor glibc's aarch64 vector math
// declarations in a device pass. These are the definitions clang's CUDA
// headers give the CUDA functions used below.
#include <__clang_cuda_builtin_vars.h>
#include <cstdint>
#define __device__ __attribute__((device))
#define __global__ __attribute__((global))
#define __constant__ __attribute__((constant))
#define __forceinline__ __inline__ __attribute__((always_inline))
#define __noinline__ __attribute__((noinline))
// as CUDA's crt/common_functions.h declares it (device calls become vprintf)
extern "C" __attribute__((host)) __device__ int printf(const char *, ...);
static __device__ __forceinline__ int atomicCAS(int *p, int cmp, int val)
{
	return __nvvm_atom_cas_gen_i(p, cmp, val);
}
static __device__ __forceinline__ int atomicExch(int *p, int val)
{
	return __nvvm_atom_xchg_gen_i(p, val);
}
static __device__ __forceinline__ unsigned int atomicAdd(unsigned int *p,
							unsigned int val)
{
	return (unsigned int)__nvvm_atom_add_gen_i((int *)p, (int)val);
}
static __device__ __forceinline__ unsigned int __activemask()
{
	unsigned int mask;
	asm volatile("activemask.b32 %0;" : "=r"(mask));
	return mask;
}
static __device__ __forceinline__ void __syncwarp(unsigned int mask)
{
	__nvvm_bar_warp_sync(mask);
}
#else
#include <__clang_cuda_builtin_vars.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cuda_runtime.h>
#include <iostream>
#include <iterator>
#include <ostream>
#include <stdio.h>
#include <string>
#include <thread>
#include <vector>

#endif

/* clang++-17 -S ./default_trampoline.cu -Wall --cuda-gpu-arch=sm_60 -O2
 * -L/usr/local/cuda/lib64/ -lcudart*/
// GPU_HELPER_MAX_BUF, HelperOperation, the request and response unions,
// CommSharedMem and gpu_ringbuf_header: shared with the host.
#include "gpu_helper_protocol.h"
using namespace bpftime_gpu;

// An ordinary (CPU) array map: its values lie in the shared segment, which the
// agent registers with CUDA, so the GPU reads and writes them directly when
// map_info gives their device address (extra_buffer); without one, through a
// host call.
const int BPF_MAP_TYPE_ARRAY = 2;
const int BPF_MAP_TYPE_GPU_HASH_MAP = 1501; // non-per-thread, single-copy shared hashmap
// IPC-based GPU maps (for x86 with CUDA IPC support)
const int BPF_MAP_TYPE_PERGPUTD_ARRAY_MAP = 1502;
const int BPF_MAP_TYPE_GPU_ARRAY_MAP = 1503; // non-per-thread, single-copy
					     // shared array
const int BPF_MAP_TYPE_GPU_RINGBUF_MAP = 1527;
const int BPF_MAP_TYPE_GPU_KERNEL_SHARED_ARRAY_MAP = 1504;

// HOST-based GPU maps (for Tegra/platforms without CUDA IPC)
const int BPF_MAP_TYPE_PERGPUTD_ARRAY_HOST_MAP = 1512;
const int BPF_MAP_TYPE_GPU_ARRAY_HOST_MAP = 1513;

struct MapBasicInfo {
	bool enabled;
	int key_size;
	int value_size;
	int max_entries;
	int map_type;
	void *extra_buffer;
	uint64_t max_thread_count;
};
__device__ __forceinline__ uint64_t read_globaltimer()
{
	uint64_t timestamp;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(timestamp));
	return timestamp;
}

__constant__ uintptr_t constData;
__constant__ MapBasicInfo map_info[1024];

static __device__ __forceinline__ void fence_system()
{
	asm volatile("membar.sys;" ::: "memory");
}
static __device__ __forceinline__ void fence_gpu()
{
	asm volatile("membar.gl;" ::: "memory");
}
static __device__ __forceinline__ unsigned int current_lane()
{
	unsigned int lane;
	asm volatile("mov.u32 %0, %%laneid;" : "=r"(lane));
	return lane;
}

// Threads of this module queue here, in GPU memory, before they take the
// process-wide lock in CommSharedMem: spinning on a word in host memory sends
// every attempt over the bus, onto the cache line of flag1 and flag2 (with 32
// warps waiting, a host call took 300 us instead of 10).
__device__ int __bpftime_module_comm_lock = 0;

// One host call: take the lock of the one request slot (CommSharedMem, shared
// by every module of the process), write the request with fill() while
// holding it, post it and wait for the response. Lanes of a warp take turns:
// without independent thread scheduling (before sm_70) a lane spinning on a
// lock that another lane of its warp holds never lets that lane run.
template <class Fill>
static __device__ __forceinline__ HelperCallResponse
host_call(long map_id, HelperOperation op, Fill fill)
{
	CommSharedMem *comm = (CommSharedMem *)constData;
	volatile CommSharedMem *shared = comm;
	const unsigned int lane = current_lane();
	HelperCallResponse resp = {};

	for (unsigned int turn = 0; turn < 32; turn++) {
		const unsigned int active = __activemask();
		if (lane == turn) {
			while (atomicCAS(&__bpftime_module_comm_lock, 0, 1) !=
			       0) {
			}
			while (atomicCAS(&comm->occupy_flag, 0, 1) != 0) {
			}
			// acquire: the request is written after the lock is held
			fence_system();
			comm->request_id = (int)op;
			comm->map_id = map_id;
			fill(comm->req);
			// release: the host reads the request after seeing flag1
			fence_system();
			shared->flag1 = 1;
			while (shared->flag2 == 0) {
			}
			shared->flag2 = 0;
			// acquire: the response is read after flag2
			fence_system();
			resp = comm->resp;
			// release: everything above happens before the next holder
			fence_system();
			atomicExch(&comm->occupy_flag, 0);
			atomicExch(&__bpftime_module_comm_lock, 0);
		}
		__syncwarp(active);
	}

	return resp;
}
extern "C" __device__ inline void simple_memcpy(void *dst, void *src, int sz)
{
	for (int i = 0; i < sz; i++)
		((char *)dst)[i] = ((char *)src)[i];
}

__device__ uint64_t getGlobalThreadId()
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	int z = blockIdx.z * blockDim.z + threadIdx.z;
	int width = gridDim.x * blockDim.x;
	int height = gridDim.y * blockDim.y;
	return ((uint64_t)z * width * height) + (y * width) + x;
}

// Set by the first thread that falls outside a GPU array map, so that the
// warning is printed once per module and not by every such thread.
__device__ int __bpftime_map_bounds_warned = 0;

static __device__ void warn_outside_map(uint64_t map, const MapBasicInfo &info,
					uint64_t tid, uint64_t key)
{
	if (atomicExch(&__bpftime_map_bounds_warned, 1) == 0) {
		printf("bpftime: GPU thread %lu, key %lu is outside map %lu (%lu threads, %d entries): lookups return NULL, updates fail. Set BPFTIME_MAP_GPU_THREAD_COUNT for the syscall server to cover every thread\n",
		       tid, key, map, info.max_thread_count, info.max_entries);
	}
}

// The calling thread's copy of entry idx in a per-thread array map, or NULL
// when the thread or the key is outside the map.
__device__ void *array_map_offset(uint64_t idx, const MapBasicInfo &info,
				  uint64_t map)
{
	const uint64_t tid = getGlobalThreadId();
	if (tid >= info.max_thread_count || idx >= (uint64_t)info.max_entries) {
		warn_outside_map(map, info, tid, idx);
		return nullptr;
	}
	return (void *)((uintptr_t)info.extra_buffer +
			idx * info.max_thread_count * info.value_size +
			tid * info.value_size);
}

// Entry idx of a single-copy array map, or NULL when the key is outside it.
static __device__ void *shared_array_offset(uint64_t idx,
					    const MapBasicInfo &info,
					    uint64_t map)
{
	if (idx >= (uint64_t)info.max_entries) {
		warn_outside_map(map, info, getGlobalThreadId(), idx);
		return nullptr;
	}
	return (char *)info.extra_buffer + idx * info.value_size;
}

static constexpr uint64_t E2BIG_RESULT = (uint64_t)-7;

extern "C" __noinline__ __device__ uint64_t _bpf_helper_ext_0001(
	uint64_t map, uint64_t key, uint64_t a, uint64_t b, uint64_t c)
{
	const auto &map_info = ::map_info[map];
	// IPC-based per-thread array map
	if (map_info.map_type == BPF_MAP_TYPE_PERGPUTD_ARRAY_MAP) {
		auto real_key = *(uint32_t *)(uintptr_t)key;
		return (uint64_t)(uintptr_t)array_map_offset(real_key, map_info,
							     map);
	}
	// Fast-path for non-per-thread GPU array map: single shared copy on
	// device-visible UVA
	if (map_info.map_type == BPF_MAP_TYPE_GPU_ARRAY_MAP ||
	    map_info.map_type == BPF_MAP_TYPE_GPU_KERNEL_SHARED_ARRAY_MAP) {
		auto real_key = *(uint32_t *)(uintptr_t)key;
		return (uint64_t)(uintptr_t)shared_array_offset(real_key,
								map_info, map);
	}
	// HOST-based per-thread array map (for Tegra)
	if (map_info.map_type == BPF_MAP_TYPE_PERGPUTD_ARRAY_HOST_MAP) {
		auto real_key = *(uint32_t *)(uintptr_t)key;
		auto offset = array_map_offset(real_key, map_info, map);
		fence_system(); // Ensure CPU writes are visible to GPU
		return (uint64_t)(uintptr_t)offset;
	}
	// HOST-based non-per-thread GPU array map (for Tegra)
	if (map_info.map_type == BPF_MAP_TYPE_GPU_ARRAY_HOST_MAP) {
		auto real_key = *(uint32_t *)(uintptr_t)key;
		auto offset = shared_array_offset(real_key, map_info, map);
		fence_system(); // Ensure CPU writes are visible to GPU
		return (uint64_t)(uintptr_t)offset;
	}
	// Ordinary array map with a device address: as the CPU programs' lookup
	// (array_map_impl::elem_lookup), NULL past max_entries.
	if (map_info.map_type == BPF_MAP_TYPE_ARRAY && map_info.extra_buffer) {
		auto real_key = *(uint32_t *)(uintptr_t)key;
		if (real_key >= (uint32_t)map_info.max_entries)
			return 0;
		fence_system(); // as the 1513 path: CPU writes before the read
		return (uint64_t)(uintptr_t)((char *)map_info.extra_buffer +
					     (uint64_t)real_key *
						     map_info.value_size);
	}
	HelperCallResponse resp = host_call(
		(long)map, HelperOperation::MAP_LOOKUP,
		[&](HelperCallRequest &req) {
			simple_memcpy(&req.map_lookup.key,
				      (void *)(uintptr_t)key,
				      map_info.key_size);
		});

	return (uintptr_t)resp.map_lookup.value;
}

extern "C" __noinline__ __device__ uint64_t _bpf_helper_ext_0002(
	uint64_t map, uint64_t key, uint64_t value, uint64_t flags, uint64_t a)
{
	const auto &map_info = ::map_info[map];
	// IPC-based per-thread array map
	if (map_info.map_type == BPF_MAP_TYPE_PERGPUTD_ARRAY_MAP) {
		auto real_key = *(uint32_t *)(uintptr_t)key;
		auto offset = array_map_offset(real_key, map_info, map);
		if (!offset)
			return E2BIG_RESULT;
		simple_memcpy(offset, (void *)(uintptr_t)value,
			      map_info.value_size);
		return 0;
	}
	// Fast-path for non-per-thread GPU array map: memcpy overwrite, system
	// fence for visibility
	if (map_info.map_type == BPF_MAP_TYPE_GPU_ARRAY_MAP ||
	    map_info.map_type == BPF_MAP_TYPE_GPU_KERNEL_SHARED_ARRAY_MAP) {
		auto real_key = *(uint32_t *)(uintptr_t)key;
		auto dst = shared_array_offset(real_key, map_info, map);
		if (!dst)
			return E2BIG_RESULT;
		simple_memcpy(dst, (void *)(uintptr_t)value,
			      map_info.value_size);
		fence_system();
		return 0;
	}
	// HOST-based per-thread array map (for Tegra)
	if (map_info.map_type == BPF_MAP_TYPE_PERGPUTD_ARRAY_HOST_MAP) {
		auto real_key = *(uint32_t *)(uintptr_t)key;
		auto offset = array_map_offset(real_key, map_info, map);
		if (!offset)
			return E2BIG_RESULT;
		simple_memcpy(offset, (void *)(uintptr_t)value,
			      map_info.value_size);
		fence_system(); // Ensure GPU writes are visible to CPU
		return 0;
	}
	// HOST-based non-per-thread GPU array map (for Tegra)
	if (map_info.map_type == BPF_MAP_TYPE_GPU_ARRAY_HOST_MAP) {
		auto real_key = *(uint32_t *)(uintptr_t)key;
		auto dst = shared_array_offset(real_key, map_info, map);
		if (!dst)
			return E2BIG_RESULT;
		simple_memcpy(dst, (void *)(uintptr_t)value,
			      map_info.value_size);
		fence_system(); // Ensure GPU writes are visible to CPU
		return 0;
	}
	// Ordinary array map with a device address: the rules of the CPU
	// programs' update (check_update_flags, array_map_impl::elem_update):
	// the low 32 bits of flags BPF_ANY, BPF_NOEXIST or BPF_EXIST (bpftime's
	// own operations may use the high ones), BPF_NOEXIST itself fails
	// (every element exists), no key past max_entries. Failures return -1,
	// as the host call and the CPU helper do.
	if (map_info.map_type == BPF_MAP_TYPE_ARRAY && map_info.extra_buffer) {
		auto real_key = *(uint32_t *)(uintptr_t)key;
		const uint64_t base_flags = flags & 0xffffffffULL;
		if (base_flags > 2 /*BPF_EXIST*/ || flags == 1 /*BPF_NOEXIST*/ ||
		    real_key >= (uint32_t)map_info.max_entries)
			return (uint64_t)-1;
		simple_memcpy((char *)map_info.extra_buffer +
				      (uint64_t)real_key * map_info.value_size,
			      (void *)(uintptr_t)value, map_info.value_size);
		fence_system(); // as the 1513 path: visible to the CPU
		return 0;
	}
	HelperCallResponse resp = host_call(
		(long)map, HelperOperation::MAP_UPDATE,
		[&](HelperCallRequest &req) {
			simple_memcpy(&req.map_update.key,
				      (void *)(uintptr_t)key,
				      map_info.key_size);
			simple_memcpy(&req.map_update.value,
				      (void *)(uintptr_t)value,
				      map_info.value_size);
			req.map_update.flags = (uintptr_t)flags;
		});
	return resp.map_update.result;
}

extern "C" __noinline__ __device__ uint64_t _bpf_helper_ext_0003(
	uint64_t map, uint64_t key, uint64_t a, uint64_t b, uint64_t c)
{
	const auto &map_info = ::map_info[map];
	// Elements of an array map cannot be deleted (array_map_impl::
	// elem_delete): -1 without asking the host.
	if (map_info.map_type == BPF_MAP_TYPE_ARRAY && map_info.extra_buffer)
		return (uint64_t)-1;
	HelperCallResponse resp = host_call(
		(long)map, HelperOperation::MAP_DELETE,
		[&](HelperCallRequest &req) {
			simple_memcpy(&req.map_delete.key,
				      (void *)(uintptr_t)key,
				      map_info.key_size);
		});
	return resp.map_delete.result;
}

extern "C" __noinline__ __device__ uint64_t
_bpf_helper_ext_0006(uint64_t fmt, uint64_t fmt_size, uint64_t arg1,
		     uint64_t arg2, uint64_t arg3)
{
	HelperCallResponse resp = host_call(
		0, HelperOperation::TRACE_PRINTK, [&](HelperCallRequest &req) {
			auto &printk = req.trace_printk;
			const char *in = (const char *)(uintptr_t)fmt;
			uint64_t size = fmt_size;
			if (size > sizeof(printk.fmt))
				size = sizeof(printk.fmt);
			for (uint64_t i = 0; i < size; i++)
				printk.fmt[i] = in[i];
			// the host prints fmt as a C string
			if (size == sizeof(printk.fmt))
				printk.fmt[size - 1] = 0;
			printk.fmt_size = (int)size;
			printk.arg1 = arg1;
			printk.arg2 = arg2;
			printk.arg3 = arg3;
		});
	return resp.trace_printk.result;
}

extern "C" __noinline__ __device__ uint64_t
_bpf_helper_ext_0014(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)
{
	HelperCallResponse resp = host_call(0,
					    HelperOperation::GET_CURRENT_PID_TGID,
					    [](HelperCallRequest &) {});
	return resp.get_tid_pgid.result;
}

// perf event output into a GPU ringbuf map: the calling thread's records
// (gpu_ringbuf_header in gpu_helper_protocol.h). Returns 0 when the record
// was written, 1 for a map or thread outside the ringbuf, 2 when the
// thread's records are full or another thread is writing to them, 3 when
// the record is larger than value_size; 2 and 3 count in `dropped`.
extern "C" __noinline__ __device__ uint64_t
_bpf_helper_ext_0025(uint64_t ctx, uint64_t map, uint64_t flags, uint64_t data,
		     uint64_t data_size)
{
	const auto &map_info = ::map_info[map];
	if (map_info.map_type != BPF_MAP_TYPE_GPU_RINGBUF_MAP) {
		printf("Calling bpf_perf_event_output on unsupported map!");
		return 1;
	}
	const uint64_t tid = getGlobalThreadId();
	if (tid >= map_info.max_thread_count) {
		// Avoid OOB writes if the map was sized for fewer threads than
		// the current kernel launch.
		return 1;
	}
	const uint64_t entries = (uint64_t)map_info.max_entries;
	const uint64_t record_size = sizeof(uint64_t) + map_info.value_size;
	auto header = (gpu_ringbuf_header *)((char *)map_info.extra_buffer +
					     tid * (sizeof(gpu_ringbuf_header) +
						    entries * record_size));
	volatile gpu_ringbuf_header *shared = header;
	if (data_size > (uint64_t)map_info.value_size) {
		atomicAdd(&header->dropped, 1u);
		return 3;
	}
	// One writer at a time: another kernel running at the same time can
	// have a thread with the same index.
	if (atomicCAS(&header->busy, 0, 1) != 0) {
		atomicAdd(&header->dropped, 1u);
		return 2;
	}
	fence_gpu(); // acquire: tail as the previous writer left it
	const uint64_t tail = shared->tail;
	const uint64_t head = shared->head;
	if (tail - head >= entries) {
		atomicAdd(&header->dropped, 1u);
		fence_gpu();
		atomicExch(&header->busy, 0);
		return 2;
	}
	// The record is written only after head was read (the branch above
	// depends on it), so the host has finished reading the record that was
	// in this place before.
	char *record = (char *)header + sizeof(gpu_ringbuf_header) +
		       (tail % entries) * record_size;
	uint64_t size = data_size;
	simple_memcpy(record, &size, sizeof(size));
	simple_memcpy(record + sizeof(uint64_t), (void *)(uintptr_t)data,
		      (int)data_size);
	// release: the host reads the record after it sees the new tail
	fence_system();
	shared->tail = tail + 1;
	// the next writer sees the new tail once it holds busy
	fence_gpu();
	atomicExch(&header->busy, 0);
	return 0;
}

extern "C" __noinline__ __device__ uint64_t
_bpf_helper_ext_0501(uint64_t data, uint64_t, uint64_t, uint64_t, uint64_t)
{
	HelperCallResponse resp = host_call(
		0, HelperOperation::PUTS, [&](HelperCallRequest &req) {
			auto &out = req.puts.data;
			const char *input = (const char *)data;
			unsigned int idx = 0;
			while (idx + 1 < sizeof(out) && input[idx]) {
				out[idx] = input[idx];
				idx++;
			}
			out[idx] = 0;
		});
	return resp.puts.result;
}

extern "C" __noinline__ __device__ uint64_t
_bpf_helper_ext_0502(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)
{
	return read_globaltimer();
}

extern "C" __noinline__ __device__ uint64_t
_bpf_helper_ext_0503(uint64_t x, uint64_t y, uint64_t z, uint64_t, uint64_t)
{
	// get block idx
	*(uint64_t *)(uintptr_t)x = blockIdx.x;
	*(uint64_t *)(uintptr_t)y = blockIdx.y;
	*(uint64_t *)(uintptr_t)z = blockIdx.z;

	return 0;
}
extern "C" __noinline__ __device__ uint64_t
_bpf_helper_ext_0504(uint64_t x, uint64_t y, uint64_t z, uint64_t, uint64_t)
{
	// get block dim
	*(uint64_t *)(uintptr_t)x = blockDim.x;
	*(uint64_t *)(uintptr_t)y = blockDim.y;
	*(uint64_t *)(uintptr_t)z = blockDim.z;

	return 0;
}
extern "C" __noinline__ __device__ uint64_t
_bpf_helper_ext_0505(uint64_t x, uint64_t y, uint64_t z, uint64_t, uint64_t)
{
	// get threadIdx
	*(uint64_t *)(uintptr_t)x = threadIdx.x;
	*(uint64_t *)(uintptr_t)y = threadIdx.y;
	*(uint64_t *)(uintptr_t)z = threadIdx.z;

	return 0;
}
extern "C" __noinline__ __device__ uint64_t
_bpf_helper_ext_0506(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)
{
	asm("membar.sys;                      \n\t");
	return 0;
}
extern "C" __noinline__ __device__ uint64_t
_bpf_helper_ext_0507(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)
{
	asm("exit;                      \n\t");
	return 0;
}
extern "C" __noinline__ __device__ uint64_t
_bpf_helper_ext_0508(uint64_t x, uint64_t y, uint64_t z, uint64_t, uint64_t)
{
	// get grid dim
	*(uint64_t *)(uintptr_t)x = gridDim.x;
	*(uint64_t *)(uintptr_t)y = gridDim.y;
	*(uint64_t *)(uintptr_t)z = gridDim.z;

	return 0;
}

extern "C" __noinline__ __device__ uint64_t
_bpf_helper_ext_0509(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)
{
    // get sm id
    uint32_t sm_id;
    asm volatile("mov.u32 %0, %%smid;" : "=r"(sm_id));
    return (uint64_t)sm_id;
}

extern "C" __noinline__ __device__ uint64_t
_bpf_helper_ext_0510(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)
{
    // get warp id
    uint32_t warp_id;
    asm volatile("mov.u32 %0, %%warpid;" : "=r"(warp_id));
    return (uint64_t)warp_id;
}

extern "C" __noinline__ __device__ uint64_t
_bpf_helper_ext_0511(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)
{
    // get lane id
    uint32_t lane_id;
    asm volatile("mov.u32 %0, %%laneid;" : "=r"(lane_id));
    return (uint64_t)lane_id;
}

#ifndef BPFTIME_TRAMPOLINE_DEVICE_ONLY
extern "C" __global__ void bpf_main(void *mem, size_t sz)
{
	printf("kernel function entered, mem=%lx, memsz=%ld\n", (uintptr_t)mem,
	       sz);
	char buf[16] = "aaa";
	printf("setup function, const data=%lx\n", constData);
	auto result = _bpf_helper_ext_0001(1ull << 32, (uintptr_t)buf, 0, 0, 0);
	_bpf_helper_ext_0002(1ull << 32, (uintptr_t)buf, (uintptr_t)buf, 0, 0);
	_bpf_helper_ext_0003(1ull << 32, (uintptr_t)buf, 0, 0, 0);
	const char msg[] = "Message from bpf: %d, %lx";
	_bpf_helper_ext_0006((uint64_t)(uintptr_t)msg, sizeof(msg), 10, 20, 0);

	printf("call done\n");
	printf("got response %d at %d\n", *(int *)result,
	       threadIdx.x + blockIdx.x * blockDim.x);
	*(int *)mem = 123;
}

static std::atomic<bool> should_exit;
void signal_handler(int)
{
	should_exit.store(true);
}
int main()
{
	signal(SIGINT, signal_handler);

	// 1. 先在主机上分配一块普通内存
	CommSharedMem *hostMem = (CommSharedMem *)malloc(sizeof(CommSharedMem));
	if (!hostMem) {
		std::cerr << "Failed to allocate hostMem\n";
		return -1;
	}

	// 2. 注册成 pinned memory (可被GPU直接访问)
	cudaError_t err = cudaHostRegister(hostMem, sizeof(CommSharedMem),
					   cudaHostRegisterMapped);
	if (err != cudaSuccess) {
		std::cerr
			<< "cudaHostRegister error: " << cudaGetErrorString(err)
			<< "\n";
		free(hostMem);
		return -1;
	}

	// 3. 获取对应的设备指针(这样DeviceKernel就能直接访问这个地址)
	CommSharedMem *devPtr = nullptr;
	err = cudaHostGetDevicePointer((void **)&devPtr, (void *)hostMem, 0);
	if (err != cudaSuccess) {
		std::cerr << "cudaHostGetDevicePointer error: "
			  << cudaGetErrorString(err) << "\n";
		cudaHostUnregister(hostMem);
		free(hostMem);
		return -1;
	}
	printf("dev ptr should be %lx, host ptr is %lx\n", (uintptr_t)devPtr,
	       (uintptr_t)hostMem);
	err = cudaMemcpyToSymbol(constData, &devPtr, sizeof(CommSharedMem *));
	if (err != cudaSuccess) {
		std::cerr << "cudaMemcpyToSymbol error: "
			  << cudaGetErrorString(err) << "\n";
		cudaHostUnregister(hostMem);
		free(hostMem);
		return -1;
	}
	int buf = 11223344;
	err = cudaHostRegister((void *)&buf, sizeof(buf),
			       cudaHostRegisterMapped);
	if (err != cudaSuccess) {
		std::cerr << "cudaHostRegister(2) error: "
			  << cudaGetErrorString(err) << " " << err << "\n";
		cudaHostUnregister(hostMem);
		free(hostMem);
		return -1;
	}
	char *devPtrStr = nullptr;
	err = cudaHostGetDevicePointer((void **)&devPtrStr, (void *)&buf, 0);
	if (err != cudaSuccess) {
		std::cerr << "cudaHostGetDevicePointer(2) error: "
			  << cudaGetErrorString(err) << "\n";
		cudaHostUnregister(hostMem);
		free(hostMem);
		return -1;
	}
	// 初始化标志位
	memset(hostMem, 0, sizeof(*hostMem));
	// 4. 启动一个线程, 模拟host侧的处理逻辑
	std::thread hostThread([&]() {
		std::cout << "[Host Thread] Start waiting...\n";

		// 这里简单用轮询，检测到flag1=1就处理
		while (!should_exit.load()) {
			if (hostMem->flag1 == 1) {
				// 清掉flag1防止重复处理
				hostMem->flag1 = 0;
				// 假设处理数据 paramA
				std::cout
					<< "[Host Thread] Got request: req_id="
					<< hostMem->request_id
					<< ", handling...\n";
				if (hostMem->request_id == 1) {
					std::cout << "call map_lookup="
						  << hostMem->req.map_lookup.key
						  << std::endl;
					// strcpy(hostMem->resp.map_lookup.value,
					//        "your value");
					hostMem->resp.map_lookup.value =
						devPtrStr;
				}
				// std::atomic_thread_fence(std::memory_order_seq_cst);

				// 处理完后, 把 flag2=1, 让设备端退出自旋
				hostMem->flag2 = 1;

				// 在实际开发中，可以加个内存栅栏，比如：
				std::atomic_thread_fence(
					std::memory_order_seq_cst);

				// 处理一次就退出本线程循环
				// break;
				std::cout << "handle done, timesum = "
					  << hostMem->time_sum[1] << std::endl;
			}

			// 为了演示，这里短暂休眠，避免100%占用CPU
			std::this_thread::sleep_for(
				std::chrono::milliseconds(10));
		}

		std::cout << "[Host Thread] Done.\n";
	});
	std::vector<MapBasicInfo> local_map_info(256);

	local_map_info[1].enabled = true;
	local_map_info[1].key_size = 16;
	local_map_info[1].value_size = 16;
	cudaMemcpyToSymbol(map_info, local_map_info.data(),
			   sizeof(MapBasicInfo) * local_map_info.size());
	// 5. 启动核函数 (只发1个block,1个thread做演示)
	bpf_main<<<1, 1>>>(hostMem, sizeof(*hostMem));

	// 等待核函数执行完毕
	cudaDeviceSynchronize();

	// 等待host线程结束
	hostThread.join();

	// 6. 收尾：解绑 pinned memory 并释放
	cudaHostUnregister(hostMem);
	free(hostMem);

	std::cout << "All done.\n";
	return 0;
}

#endif

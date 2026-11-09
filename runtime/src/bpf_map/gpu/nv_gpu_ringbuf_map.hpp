#ifndef _NV_GPU_RINGBUF_MAP_HPP
#define _NV_GPU_RINGBUF_MAP_HPP

#include "bpf_map/map_common_def.hpp"
#include "cuda.h"
#include "trampoline/gpu_helper_protocol.h"
#include <cstddef>
#include <functional>
#include <utility>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/unordered_map.hpp>
namespace bpftime
{
using pid_devptr_map_value_ty = std::pair<const int, CUdeviceptr>;
using pid_devptr_map_allocator = boost::interprocess::allocator<
	pid_devptr_map_value_ty,
	boost::interprocess::managed_shared_memory::segment_manager>;
using pid_devptr_map =
	boost::unordered_map<int, CUdeviceptr, int_hasher, std::equal_to<int>,
			     pid_devptr_map_allocator>;

// Written by the GPU trampoline (_bpf_helper_ext_0025); the protocol is
// described in attach/nv_attach_impl/trampoline/gpu_helper_protocol.h.
using ringbuf_header = bpftime_gpu::gpu_ringbuf_header;

class nv_gpu_ringbuf_map_impl {
	/**
	    Memory layout of each thread:
	    |struct ringbuf_header|(uint64_t)size of data in page 0|page0 (in
	   size of value size)|(uint64_t) size of data in page 1|page1 (in size
	   of value size)|.........
	    */

	// CUdeviceptr server_gpu_memory;
	bytes_vec data_buffer;
	pid_devptr_map agent_gpu_shared_mem;
	uint64_t value_size;
	uint64_t max_entries;
	uint64_t thread_count;

	bytes_vec local_buffer;

	uint64_t entry_size;

	// sum of the threads' `dropped` counters at the last warning
	uint64_t reported_drops = 0;

    public:
	const static bool should_lock = true;
	nv_gpu_ringbuf_map_impl(
		boost::interprocess::managed_shared_memory &memory,

		uint64_t value_size, uint64_t max_entries,
		uint64_t thread_count);

	void *elem_lookup(const void *key);

	long elem_update(const void *key, const void *value, uint64_t flags);

	long elem_delete(const void *key);

	int map_get_next_key(const void *key, void *next_key);
	// Only to be called at syscall server side, by one thread at a time.
	// Passes every record the GPU has published to fn, oldest first per
	// GPU thread, and frees its place. Returns the number of records.
	int
	drain_data(const std::function<void(const void *, uint64_t size)> &fn);

	// The records of every GPU thread, in this process's address space:
	// thread i's ringbuf_header is at i * thread_records_size().
	char *host_data_buffer()
	{
		return (char *)data_buffer.data();
	}
	uint64_t thread_records_size() const
	{
		return entry_size;
	}

	CUdeviceptr get_gpu_mem_buffer()
	{
		return try_initialize_for_agent_and_get_mapped_address();
	}
	uint64_t get_max_thread_count() const
	{
		return thread_count;
	}
	virtual ~nv_gpu_ringbuf_map_impl();

	CUdeviceptr try_initialize_for_agent_and_get_mapped_address();
};
} // namespace bpftime

#endif

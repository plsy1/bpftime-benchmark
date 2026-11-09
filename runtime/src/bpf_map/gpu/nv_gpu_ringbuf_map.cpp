#include "nv_gpu_ringbuf_map.hpp"
#include "bpftime_shm_internal.hpp"
#include "cuda.h"
#include "spdlog/spdlog.h"
#include <atomic>
#include <cerrno>
#include <cstring>
#include <stdexcept>
using namespace bpftime;

nv_gpu_ringbuf_map_impl::nv_gpu_ringbuf_map_impl(
	boost::interprocess::managed_shared_memory &memory, uint64_t value_size,
	uint64_t max_entries, uint64_t thread_count)
	: data_buffer(memory.get_segment_manager()),
	  agent_gpu_shared_mem(memory.get_segment_manager()),
	  value_size(value_size), max_entries(max_entries),
	  thread_count(thread_count), local_buffer(memory.get_segment_manager())
{
	entry_size = (value_size + sizeof(uint64_t)) * max_entries +
		     sizeof(ringbuf_header);
	local_buffer.resize(entry_size);
	data_buffer.resize(entry_size * thread_count);
}

CUdeviceptr
nv_gpu_ringbuf_map_impl::try_initialize_for_agent_and_get_mapped_address()
{
	if (shm_holder.global_shared_memory.get_open_type() !=
	    shm_open_type::SHM_REMOVE_AND_CREATE) {
		int pid = getpid();
		if (auto itr = agent_gpu_shared_mem.find(pid);
		    itr == agent_gpu_shared_mem.end()) {
			SPDLOG_INFO(
				"Initializing nv_gpu_ringbuf_map_impl at pid {}",
				pid);
			CUdeviceptr device_ptr = 0;
			if (auto err = cuMemHostGetDevicePointer(
				    &device_ptr, (void *)data_buffer.data(), 0);
			    err != CUDA_SUCCESS) {
				SPDLOG_ERROR(
					"Unable to map host ringbuf buffer into device address space, error={}",
					(int)err);
				throw std::runtime_error(
					"Unable to map host ringbuf buffer into device address space");
			}
			SPDLOG_INFO("Mapped GPU memory for gpu ringbuf map: {}",
				    (uintptr_t)device_ptr);
			agent_gpu_shared_mem[pid] = device_ptr;
		}
		return agent_gpu_shared_mem[pid];
	} else {
		return (CUdeviceptr)data_buffer.data();
	}
}

int nv_gpu_ringbuf_map_impl::drain_data(
	const std::function<void(const void *, uint64_t)> &fn)
{
	const uint64_t record_size = value_size + sizeof(uint64_t);
	int records = 0;
	uint64_t drops = 0;
	for (uint64_t i = 0; i < thread_count; i++) {
		auto header = (ringbuf_header *)(uintptr_t)(data_buffer.data() +
							    i * entry_size);
		// acquire: the GPU wrote the records below tail before tail
		// (membar.sys)
		const uint64_t tail =
			__atomic_load_n(&header->tail, __ATOMIC_ACQUIRE);
		uint64_t head = __atomic_load_n(&header->head, __ATOMIC_RELAXED);
		if (tail - head > max_entries) {
			SPDLOG_WARN(
				"GPU ringbuf thread {}: head {} and tail {} are more than {} records apart; skipping to the last {}",
				i, head, tail, max_entries, max_entries);
			head = tail - max_entries;
		}
		for (; head != tail; head++) {
			auto record = (const char *)header +
				      sizeof(ringbuf_header) +
				      (head % max_entries) * record_size;
			uint64_t size;
			memcpy(&size, record, sizeof(size));
			if (size > value_size) {
				SPDLOG_WARN(
					"GPU ringbuf thread {}: record of {} bytes in a map of {}-byte values; passing {}",
					i, size, value_size, value_size);
				size = value_size;
			}
			fn(record + sizeof(uint64_t), size);
			records++;
			// release: the record has been read before the GPU
			// may write its place again
			__atomic_store_n(&header->head, head + 1,
					 __ATOMIC_RELEASE);
		}
		drops += __atomic_load_n(&header->dropped, __ATOMIC_RELAXED);
	}
	if (drops > reported_drops) {
		SPDLOG_WARN(
			"GPU ringbuf: the GPU dropped {} records since the last poll ({} in all): full, larger than the {}-byte value size, or written by two threads at once",
			drops - reported_drops, drops, value_size);
		reported_drops = drops;
	}
	return records;
}

nv_gpu_ringbuf_map_impl::~nv_gpu_ringbuf_map_impl()
{
}

void *nv_gpu_ringbuf_map_impl::elem_lookup(const void *key)
{
	SPDLOG_ERROR("Element lookup is not supported by gpu ringbuf map");
	errno = -ENOTSUP;
	return nullptr;
}

long nv_gpu_ringbuf_map_impl::elem_update(const void *key, const void *value,
					  uint64_t flags)
{
	SPDLOG_ERROR("Element update is not supported by gpu ringbuf map");
	errno = -ENOTSUP;
	return -1;
}

long nv_gpu_ringbuf_map_impl::elem_delete(const void *key)
{
	SPDLOG_ERROR("Element delete is not supported by gpu ringbuf map");
	errno = -ENOTSUP;
	return -1;
}

int nv_gpu_ringbuf_map_impl::map_get_next_key(const void *key, void *next_key)
{
	SPDLOG_ERROR("Get next key is not supported by gpu ringbuf map");
	errno = -ENOTSUP;
	return -1;
}

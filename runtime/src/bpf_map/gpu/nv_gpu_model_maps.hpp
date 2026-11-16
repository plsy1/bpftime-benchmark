#ifndef _NV_GPU_MODEL_MAPS_HPP
#define _NV_GPU_MODEL_MAPS_HPP
// Maps of the per-thread map model (prototype, branch proto/map-model).
//
// 1530 thread-local array: each GPU thread's own copy lives in its local
// memory (a kernel-scope .local the agent's routing adds to every probed
// kernel, zeroed when the kernel starts). Nothing is stored here: the object
// only describes the map, and the CPU sees no values (lookups: ENOENT).
//
// 1531 sum array and 1532 device per-thread array: u64 words in device
// memory, which GPU programs may change with 64-bit atomic adds only (XADD).
// The agent copies them into this object's shared values (delivery):
//   1531: max_entries values, each word summed over the (SM, lane) shards
//         the GPU spread its adds over;
//   1532: as 1502/1512, a value per key and thread index ([key][tid]).
// A lookup of key max_entries + i returns status word i (then zeros up to a
// value): 0 deliveries made, 1 1 once the final delivery at process exit
// completed, 2 pid of the delivering process.
#include "bpf_map/map_common_def.hpp"
#include "cuda.h"
#include <cstdint>

namespace bpftime
{
class nv_gpu_thread_local_map_impl {
	uint64_t value_size;
	uint64_t max_entries;

    public:
	const static bool should_lock = false;
	nv_gpu_thread_local_map_impl(
		boost::interprocess::managed_shared_memory &memory,
		uint64_t value_size, uint64_t max_entries);
	void *elem_lookup(const void *key);
	long elem_update(const void *key, const void *value, uint64_t flags);
	long elem_delete(const void *key);
	int map_get_next_key(const void *key, void *next_key);
	uint64_t get_value_size() const
	{
		return value_size;
	}
	uint64_t get_max_entries() const
	{
		return max_entries;
	}
};

enum : uint64_t {
	GPU_MODEL_STATUS_DELIVERIES = 0,
	GPU_MODEL_STATUS_FINAL = 1,
	GPU_MODEL_STATUS_PID = 2,
	GPU_MODEL_STATUS_WORDS = 8,
};

class nv_gpu_device_array_map_impl {
	// delivered values: 1531 [key][value], 1532 [key][tid][value]
	bytes_vec data_buffer;
	// GPU_MODEL_STATUS_WORDS u64 words, plus a zeroed tail up to value_size
	bytes_vec status_buffer;
	uint64_t value_size;
	uint64_t max_entries;
	// 1532: thread indices per key; 1531: 1
	uint64_t thread_count;
	bool per_thread;
	// 1531: shards per key, set by the agent that allocated the device
	// copy (one GPU process per segment; 0 until then)
	uint64_t shards = 0;

    public:
	const static bool should_lock = false;
	nv_gpu_device_array_map_impl(
		boost::interprocess::managed_shared_memory &memory,
		uint64_t value_size, uint64_t max_entries,
		uint64_t thread_count, bool per_thread);
	void *elem_lookup(const void *key);
	long elem_update(const void *key, const void *value, uint64_t flags);
	long elem_delete(const void *key);
	int map_get_next_key(const void *key, void *next_key);

	// Agent side: the device copy (allocated and registered for delivery
	// at the first call in this process); 0 when it cannot be made.
	CUdeviceptr get_gpu_mem_buffer();
	// 1531: shards; 1532: thread indices per key
	uint64_t get_max_thread_count() const
	{
		return per_thread ? thread_count : shards;
	}
	bool is_per_thread() const
	{
		return per_thread;
	}
	uint64_t get_value_size() const
	{
		return value_size;
	}
	uint64_t get_max_entries() const
	{
		return max_entries;
	}
	// bytes of the CPU's copy (all keys)
	uint64_t host_bytes() const
	{
		return data_buffer.size();
	}
	uint64_t *host_words()
	{
		return (uint64_t *)data_buffer.data();
	}
	uint64_t *status_words()
	{
		return (uint64_t *)status_buffer.data();
	}
	void set_shards(uint64_t s)
	{
		shards = s;
	}
};

namespace gpu_model
{
// Delivery (agent): copies every registered device array into its shared
// values, from a thread of its own (relaxed capture mode, own non-blocking
// stream): a snapshot kernel reads each word with ld.relaxed.gpu (morally
// strong with the programs' gpu-scope atomic adds: no data race, PTX ISA
// 8.7) and sums the shards into a staging buffer that only it writes; after
// that kernel completed (stream synchronize) the staging buffer is copied
// to the host, then into the shared values. A final delivery runs at
// process exit (atexit), after the application synchronized.
void register_device_array(nv_gpu_device_array_map_impl *map,
			   CUdeviceptr device, uint64_t entries,
			   uint64_t shards, uint64_t words_per_entry);
// One delivery of every registered map now, on the calling thread; final
// marks the maps' status. Returns the number of maps delivered.
int deliver_now(bool final);
} // namespace gpu_model
} // namespace bpftime

#endif

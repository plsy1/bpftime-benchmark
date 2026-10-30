/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#ifndef _BPFTIME_PER_CPU_ARRAY_MAP_HPP
#define _BPFTIME_PER_CPU_ARRAY_MAP_HPP
#include "bpf_map/map_common_def.hpp"
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/smart_ptr/unique_ptr.hpp>
#include <boost/interprocess/containers/vector.hpp>

#include <cerrno>
#include <cstdint>
namespace bpftime
{

class per_cpu_array_map_impl {
	bytes_vec data;

	int ncpu;
	uint32_t value_size;
	uint32_t max_ent;
	uint8_t *data_at(size_t idx, size_t cpu)
	{
		return data.data() + idx * value_size * ncpu + cpu * value_size;
	}
	long elem_update_checked(const void *key, const void *value,
				 uint64_t flags);

    public:
	const static bool should_lock = false;
	uint8_t *get_raw_data() { return data.data(); }
	uint32_t get_value_size() const { return value_size; }
	uint32_t get_max_entries() const { return max_ent; }
	uint32_t get_cpu_count() const { return ncpu > 0 ? ncpu : 0; }
	// JIT bindings use the canonical implementation dimensions, not FD attrs.
	void *lookup_bound(const void *key)
	{
		const int cpu = my_sched_getcpu();
		uint32_t index;
		if (!key || cpu < 0 || cpu >= ncpu) {
			errno = ENOENT;
			return nullptr;
		}
		__builtin_memcpy(&index, key, sizeof(index));
		if (index >= max_ent) {
			errno = ENOENT;
			return nullptr;
		}
		return data_at(index, cpu);
	}

	per_cpu_array_map_impl(
		boost::interprocess::managed_shared_memory &memory,
		uint32_t value_size, uint32_t max_entries, uint32_t ncpu);
	per_cpu_array_map_impl(
		boost::interprocess::managed_shared_memory &memory,
		uint32_t value_size, uint32_t max_entries);

	void *elem_lookup(const void *key);
	// Production lookup path without diagnostic A/B mode dispatch. Callers
	// must select this path only when the runtime is in the base mode.
	__attribute__((always_inline)) void *elem_lookup_base(const void *key)
	{
		return ensure_on_current_cpu<void *>([&](int cpu) -> void * {
			if (key == nullptr ||
			    __builtin_expect((unsigned)cpu >= (unsigned)ncpu, 0)) {
				errno = ENOENT;
				return nullptr;
			}
			uint32_t key_val = *(uint32_t *)key;
			if (key_val >= max_ent) {
				errno = ENOENT;
				return nullptr;
			}
			return data_at(key_val, cpu);
		});
	}

	long elem_update(const void *key, const void *value, uint64_t flags);
	// Production update path without diagnostic A/B mode dispatch. Callers
	// must select this path only when the runtime is in the base mode.
	long elem_update_base(const void *key, const void *value, uint64_t flags);

	long elem_delete(const void *key);

	int map_get_next_key(const void *key, void *next_key);

	void *elem_lookup_userspace(const void *key);

	long elem_update_userspace(const void *key, const void *value,
				   uint64_t flags);

	long elem_delete_userspace(const void *key);
};
} // namespace bpftime

#endif

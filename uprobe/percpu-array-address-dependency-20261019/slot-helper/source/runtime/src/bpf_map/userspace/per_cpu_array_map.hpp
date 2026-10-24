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
	// Keep the fully prepared address opaque to the CPU-query function so
	// optimization cannot move its map loads back across the query.
	__attribute__((noinline)) static void *lookup_cpu_slot(
		uint8_t *entry_base, size_t cpu_stride);
	long elem_update_checked(const void *key, const void *value,
				 uint64_t flags);

    public:
	const static bool should_lock = false;

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
		if (key == nullptr) {
			return ensure_on_current_cpu<void *>([](int) -> void * {
				errno = ENOENT;
				return nullptr;
			});
		}
		const uint32_t key_val = *(const uint32_t *)key;
		if (key_val >= max_ent) {
			return ensure_on_current_cpu<void *>([](int) -> void * {
				errno = ENOENT;
				return nullptr;
			});
		}
		// Keep only the selected entry's base and value width live across
		// the CPU query, rather than the map and key plus later loads.
		auto *entry_base = data_at(key_val, 0);
		const size_t cpu_stride = value_size;
		return lookup_cpu_slot(entry_base, cpu_stride);
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

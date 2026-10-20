/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#ifndef _ARRAY_MAP_HPP
#define _ARRAY_MAP_HPP
#include <bpf_map/map_common_def.hpp>

namespace bpftime
{

namespace detail
{
// Keep errno access off successful array lookups so callers can remain
// leaf functions without saving registers for an error-only call.
__attribute__((cold, noinline)) void *array_lookup_enoent();
} // namespace detail

// implementation of array map
class array_map_impl {
	bytes_vec data;
	uint32_t _value_size;
	uint32_t _max_entries;

    public:
	const static bool should_lock = false;
	array_map_impl(boost::interprocess::managed_shared_memory &memory,
		       uint32_t value_size, uint32_t max_entries);

	void *elem_lookup(const void *key);

	long elem_update(const void *key, const void *value, uint64_t flags);

	long elem_delete(const void *key);

	int map_get_next_key(const void *key, void *next_key);

	void *get_raw_data() const;
	uint32_t get_value_size() const { return _value_size; }
	uint32_t get_max_entries() const { return _max_entries; }
	friend class stack_trace_map_impl;
};

} // namespace bpftime
#endif

/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#ifndef _STACK_TRACE_MAP_HPP
#define _STACK_TRACE_MAP_HPP
#include <boost/container_hash/hash.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <bpf_map/map_common_def.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace bpftime
{
namespace stack_trace_map
{

static inline uint64_t hash_stack_trace(const uint64_t *ips, size_t nr)
{
	using boost::hash_combine;
	size_t seed = 0;
	hash_combine(seed, nr);
	for (size_t i = 0; i < nr; i++)
		hash_combine(seed, ips[i]);
	return seed;
}
template <class T> static inline uint64_t hash_stack_trace(const T &stk)
{
	return hash_stack_trace(stk.data(), stk.size());
}

} // namespace stack_trace_map

// One slot per stack id, as the kernel's buckets. A slot's record (hash,
// frame count, frames) is allocated under the map lock when the id is first
// filled and kept until the map is destroyed (a delete empties it), so
// find_stack_trace, without the lock, never follows a freed record. The
// frame count is the record's validity word: a writer clears it before
// changing the record and stores it last (release).
class stack_trace_map_impl {
	struct slot {
		// Byte offset of the record from this slot, 0 before the first
		// fill: each process maps the segment at its own address.
		int64_t record;
	};
	using slot_allocator = boost::interprocess::allocator<
		slot, boost::interprocess::managed_shared_memory::segment_manager>;
	// Record words: hash, frame count, then max_stack_entries frames
	// (zero past the count).
	enum { REC_HASH = 0, REC_NR = 1, REC_FRAMES = 2 };

	uint64_t max_stack_entries;
	uint64_t max_entries;
	boost::interprocess::vector<slot, slot_allocator> slots;
	uint64_vec_allocator record_allocator;

	uint64_t *record_of(const slot &s) const;
	uint64_t *ensure_record(slot &s);
	void store_trace(uint64_t *record, const uint64_t *ips, size_t nr,
			 uint64_t hash);

    public:
	const static bool should_lock = true;
	stack_trace_map_impl(boost::interprocess::managed_shared_memory &memory,
			     uint32_t key_size, uint32_t value_size,
			     uint32_t max_entries);
	~stack_trace_map_impl();

	void *elem_lookup(const void *key);

	long elem_update(const void *key, const void *value, uint64_t flags);

	long elem_delete(const void *key);

	int map_get_next_key(const void *key, void *next_key);

	// Without the map lock: the id of this trace when its slot holds it,
	// else -1 (take the lock and call fill_stack_trace, which decides).
	int find_stack_trace(const uint64_t *ips, size_t nr,
			     bool compare_only_by_hash) const;

	// With the map lock held.
	int fill_stack_trace(const uint64_t *ips, size_t nr,
			     bool discard_old_one, bool compare_only_by_hash);
	int fill_stack_trace(const std::vector<uint64_t> &stk,
			     bool discard_old_one, bool compare_only_by_hash)
	{
		return fill_stack_trace(stk.data(), stk.size(), discard_old_one,
					compare_only_by_hash);
	}
};

} // namespace bpftime
#endif

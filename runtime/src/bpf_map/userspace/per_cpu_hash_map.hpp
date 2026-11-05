/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#ifndef _BPFTIME_PER_CPU_HASH_MAP_HPP
#define _BPFTIME_PER_CPU_HASH_MAP_HPP
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <bpf_map/map_common_def.hpp>
#include <bpf_map/bpftime_hash_map.hpp>
#include <cstdint>

namespace bpftime
{

// A per-CPU hash: the keys in a bpftime_hash_map (lookups take no lock,
// writers lock stripes of buckets), whose four-byte value is the number of
// the key's value slot. A slot holds the values of every CPU, value_size
// bytes each, packed in CPU order (the kernel's per-element layout).
//
// The slots live in a pool of chunks, allocated as slots are first used and
// never given back to the segment: a program keeps a value pointer after a
// delete, and its writes must land in a slot (as with the kernel's
// preallocated htab, possibly another key's), not in memory the segment
// allocator handed out again. The pool's free list has a spinlock, taken
// only to insert a new key or to delete one; lookups and updates of an
// existing key never take it (BPF_NOEXIST finds the key by a lookup first; a
// key inserted between that lookup and the insert costs a slot taken and
// given back).
//
// An existing key's value is written under its stripe locks
// (with_existing_value), so a concurrent delete cannot give its slot to
// another key meanwhile; a lookup reads the slot number inside the
// lookup's check (lookup_read) for the same reason.
class per_cpu_hash_map_impl {
	using segment_manager_t =
		boost::interprocess::managed_shared_memory::segment_manager;
	using int64_vec_allocator =
		boost::interprocess::allocator<int64_t, segment_manager_t>;
	using int64_vec =
		boost::interprocess::vector<int64_t, int64_vec_allocator>;
	using uint32_vec_allocator =
		boost::interprocess::allocator<uint32_t, segment_manager_t>;
	using uint32_vec =
		boost::interprocess::vector<uint32_t, uint32_vec_allocator>;

	bpftime_hash_map table;
	uint32_t key_size;
	uint32_t value_size;
	int ncpu;
	uint32_t slot_shift; // log2 of the slots per chunk
	uint32_t slot_capacity;
	size_t slot_bytes; // value_size * ncpu
	boost::interprocess::offset_ptr<segment_manager_t> manager;
	// Chunk start minus this object's address (the same in every process),
	// 0 while not allocated. Fixed size; an entry is set once, under the
	// pool lock, before any slot in its chunk is handed out.
	int64_vec chunk_offsets;
	uint32_vec free_next;

	char _read_mostly_end[64] = {};
	pthread_spinlock_t pool_lock;
	uint32_t free_head;
	uint32_t slot_count; // slots handed out at least once
	char _pool_end[64] = {};

	// sched_getcpu() can fail (-1) and, with the count fixed at creation,
	// must not index past it.
	bool valid_cpu(int cpu) const
	{
		// One unsigned compare covers -1 too.
		return __builtin_expect((unsigned)cpu < (unsigned)ncpu, 1);
	}
	uint8_t *slot_data(uint32_t slot) const
	{
		const int64_t offset = chunk_offsets.data()[slot >> slot_shift];
		return (uint8_t *)this + offset +
		       (size_t)(slot & ((1u << slot_shift) - 1)) * slot_bytes;
	}
	static uint32_t slot_of(const void *table_value)
	{
		return std::atomic_ref<uint32_t>(
			       *(uint32_t *)const_cast<void *>(table_value))
			.load(std::memory_order_acquire);
	}
	// A slot with every CPU's value zero; false with errno set when none
	// is left (E2BIG) or a chunk cannot be allocated (ENOMEM).
	bool acquire_slot(uint32_t &slot);
	void release_slot(uint32_t slot);
	// Shared by the program and bpf() updates: write(values) fills the
	// values of an existing key, or of a new key's zeroed slot.
	template <class Write>
	long update_with(const void *key, uint64_t flags, Write &&write);

    public:
	const static bool should_lock = false;

	per_cpu_hash_map_impl(boost::interprocess::managed_shared_memory &memory,
			      uint32_t key_size, uint32_t value_size,
			      uint32_t max_entries, bool zero_seed = false);
	per_cpu_hash_map_impl(boost::interprocess::managed_shared_memory &memory,
			      uint32_t key_size, uint32_t value_size,
			      uint32_t max_entries, int ncpu,
			      bool zero_seed = false);
	~per_cpu_hash_map_impl();
	per_cpu_hash_map_impl(const per_cpu_hash_map_impl &) = delete;
	per_cpu_hash_map_impl &operator=(const per_cpu_hash_map_impl &) = delete;

	void *elem_lookup(const void *key);

	long elem_update(const void *key, const void *value, uint64_t flags);

	long elem_delete(const void *key);

	int map_get_next_key(const void *key, void *next_key);

	void *elem_lookup_userspace(const void *key);

	long elem_update_userspace(const void *key, const void *value,
				   uint64_t flags);

	long elem_delete_userspace(const void *key);
	long lookup_and_delete_userspace(const void *key, void *value);
	uint32_t get_value_size() const
	{
		return value_size;
	}
	int getncpu() const
	{
		return ncpu;
	}

	// Tests: the key table, and holding the pool's lock.
	bpftime_hash_map &table_for_test()
	{
		return table;
	}
	void lock_pool_for_test()
	{
		bpftime_spin_lock(&pool_lock);
	}
	void unlock_pool_for_test()
	{
		bpftime_spin_unlock(&pool_lock);
	}
};
} // namespace bpftime

#endif

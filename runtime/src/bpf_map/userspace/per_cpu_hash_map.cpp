/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#include "bpf_map/map_common_def.hpp"
#include "linux/bpf.h"
#include "spdlog/spdlog.h"
#include <bpf_map/userspace/per_cpu_hash_map.hpp>
#include <cerrno>
#include <cstring>
#include <limits>
#include <new>
#include "platform_utils.hpp"

namespace bpftime
{
namespace
{
constexpr uint32_t invalid_slot = std::numeric_limits<uint32_t>::max();
constexpr uint64_t target_chunk_bytes = 64ULL << 10;
constexpr uint32_t max_slot_shift = 8; // 256 slots per chunk
constexpr size_t chunk_align = 64;

uint32_t choose_slot_shift(uint64_t slot_bytes)
{
	uint32_t shift = 0;
	while (shift < max_slot_shift &&
	       (uint64_t{ 2 } << shift) * slot_bytes <= target_chunk_bytes)
		shift++;
	return shift;
}
} // namespace

per_cpu_hash_map_impl::per_cpu_hash_map_impl(
	boost::interprocess::managed_shared_memory &memory, uint32_t key_size,
	uint32_t value_size, uint32_t max_entries)
	: per_cpu_hash_map_impl(memory, key_size, value_size, max_entries,
				bpftime_cpu_index_limit())
{
}

per_cpu_hash_map_impl::per_cpu_hash_map_impl(
	boost::interprocess::managed_shared_memory &memory, uint32_t key_size,
	uint32_t value_size, uint32_t max_entries, int ncpu)
	: table(memory, max_entries, key_size, sizeof(uint32_t)),
	  key_size(key_size), value_size(value_size), ncpu(ncpu),
	  slot_shift(choose_slot_shift((uint64_t)value_size * ncpu)),
	  // Inserts that lose a race to insert the same key hold a slot for a
	  // moment: a few spare slots keep a nearly full map from failing for
	  // that (the kernel's percpu htab has extra elements too).
	  slot_capacity(max_entries > invalid_slot - (uint32_t)ncpu ?
				invalid_slot - 1 :
				max_entries + (uint32_t)ncpu),
	  slot_bytes((size_t)value_size * ncpu),
	  manager(memory.get_segment_manager()),
	  chunk_offsets(memory.get_segment_manager()),
	  free_next(memory.get_segment_manager()), free_head(invalid_slot),
	  slot_count(0)
{
	SPDLOG_DEBUG(
		"Initializing per cpu hash, key size {}, value size {}, ncpu {}",
		key_size, value_size, ncpu);
	const size_t chunks =
		((size_t)slot_capacity + (size_t{ 1 } << slot_shift) - 1) >>
		slot_shift;
	chunk_offsets.resize(chunks, 0);
	free_next.resize(slot_capacity, invalid_slot);
	pthread_spin_init(&pool_lock, PTHREAD_PROCESS_SHARED);
}

per_cpu_hash_map_impl::~per_cpu_hash_map_impl()
{
	for (int64_t offset : chunk_offsets)
		if (offset != 0)
			manager->deallocate((uint8_t *)this + offset);
	pthread_spin_destroy(&pool_lock);
}

bool per_cpu_hash_map_impl::acquire_slot(uint32_t &slot)
{
	bpftime_spin_lock(&pool_lock);
	if (free_head != invalid_slot) {
		slot = free_head;
		free_head = free_next[slot];
	} else if (slot_count < slot_capacity) {
		slot = slot_count;
		auto &offset = chunk_offsets[slot >> slot_shift];
		if (offset == 0) {
			// Rare (once per chunk), so allocating under the
			// spinlock is fine.
			void *chunk = manager->allocate_aligned(
				slot_bytes << slot_shift, chunk_align,
				std::nothrow);
			if (chunk == nullptr) {
				bpftime_spin_unlock(&pool_lock);
				errno = ENOMEM;
				return false;
			}
			offset = (uint8_t *)chunk - (uint8_t *)this;
		}
		slot_count++;
	} else {
		bpftime_spin_unlock(&pool_lock);
		errno = E2BIG;
		return false;
	}
	bpftime_spin_unlock(&pool_lock);
	std::memset(slot_data(slot), 0, slot_bytes);
	return true;
}

void per_cpu_hash_map_impl::release_slot(uint32_t slot)
{
	bpftime_spin_lock(&pool_lock);
	free_next[slot] = free_head;
	free_head = slot;
	bpftime_spin_unlock(&pool_lock);
}

// Kernel percpu htab: BPF_NOEXIST on an existing key fails with EEXIST,
// BPF_EXIST on a missing key with ENOENT, a new key in a full map with
// E2BIG; a program writes only its CPU's value, and a new key's other CPUs
// start at zero (pcpu_init_value).
template <class Write>
long per_cpu_hash_map_impl::update_with(const void *key, uint64_t flags,
					Write &&write)
{
	using update_mode = bpftime_hash_map::update_mode;
	using update_result = bpftime_hash_map::update_result;
	// The low 32 bits: the high ones are bpftime's.
	const uint64_t base_flags = flags & 0xFFFFFFFFULL;
	for (;;) {
		if (base_flags != BPF_NOEXIST) {
			if (table.with_existing_value(key, [&](void *value) {
				    write(slot_data(slot_of(value)));
			    }))
				return 0;
			if (base_flags == BPF_EXIST) {
				errno = ENOENT;
				return -1;
			}
		}
		uint32_t slot;
		if (!acquire_slot(slot))
			return -1;
		write(slot_data(slot));
		// Publishing the key publishes the slot's values with it.
		switch (table.update(key, &slot, update_mode::no_exist)) {
		case update_result::ok:
			return 0;
		case update_result::exists:
			release_slot(slot);
			if (base_flags == BPF_NOEXIST) {
				errno = EEXIST;
				return -1;
			}
			continue; // Inserted meanwhile: update it.
		case update_result::missing: // Not with no_exist.
		case update_result::full:
			break;
		}
		release_slot(slot);
		errno = E2BIG;
		return -1;
	}
}

void *per_cpu_hash_map_impl::elem_lookup(const void *key)
{
	int cpu = my_sched_getcpu();
	if (key == nullptr || !valid_cpu(cpu)) {
		errno = ENOENT;
		return nullptr;
	}
	uint32_t slot = 0;
	if (table.lookup_read(key, [&](const void *value) {
		    slot = slot_of(value);
	    }) == nullptr) {
		errno = ENOENT;
		return nullptr;
	}
	return slot_data(slot) + (size_t)value_size * cpu;
}

long per_cpu_hash_map_impl::elem_update(const void *key, const void *value,
					uint64_t flags)
{
	if (!check_update_flags(flags))
		return -1;
	int cpu = my_sched_getcpu();
	if (!valid_cpu(cpu)) {
		errno = EINVAL;
		return -1;
	}
	SPDLOG_DEBUG("Run per cpu hash update at cpu {}", cpu);
	return update_with(key, flags, [&](uint8_t *values) {
		std::memcpy(values + (size_t)value_size * cpu, value,
			    value_size);
	});
}

long per_cpu_hash_map_impl::elem_delete(const void *key)
{
	int cpu = my_sched_getcpu();
	if (!valid_cpu(cpu)) {
		errno = EINVAL;
		return -1;
	}
	return elem_delete_userspace(key);
}

int per_cpu_hash_map_impl::map_get_next_key(const void *key, void *next_key)
{
	if (next_key == nullptr) {
		errno = EINVAL;
		return -1;
	}
	if (!table.next_key(key, next_key)) {
		errno = ENOENT; // empty map or the last key
		return -1;
	}
	return 0;
}

// Copied under the key's stripe locks, so that a delete and another key's
// insert cannot hand the caller that key's values meanwhile.
void *per_cpu_hash_map_impl::elem_lookup_userspace(const void *key)
{
	if (key == nullptr) {
		errno = ENOENT;
		return nullptr;
	}
	uint8_t *out = percpu_userspace_buffer(percpu_userspace_size(value_size));
	if (!table.with_existing_value(key, [&](void *value) {
		    percpu_values_to_userspace(out, slot_data(slot_of(value)),
					       value_size, ncpu);
	    })) {
		errno = ENOENT;
		return nullptr;
	}
	return out;
}

long per_cpu_hash_map_impl::elem_update_userspace(const void *key,
						  const void *value,
						  uint64_t flags)
{
	if (!check_update_flags(flags))
		return -1;
	return update_with(key, flags, [&](uint8_t *values) {
		percpu_values_from_userspace(values, (const uint8_t *)value,
					     value_size, ncpu);
	});
}

long per_cpu_hash_map_impl::elem_delete_userspace(const void *key)
{
	uint32_t slot;
	if (!table.elem_delete(key, &slot)) {
		errno = ENOENT;
		return -1;
	}
	release_slot(slot);
	return 0;
}

long per_cpu_hash_map_impl::lookup_and_delete_userspace(const void *key,
							void *value)
{
	uint32_t slot;
	if (!table.elem_delete(key, &slot)) {
		errno = ENOENT;
		return -1;
	}
	// The slot is no key's until it is released.
	percpu_values_to_userspace((uint8_t *)value, slot_data(slot), value_size,
				   ncpu);
	release_slot(slot);
	return 0;
}
} // namespace bpftime

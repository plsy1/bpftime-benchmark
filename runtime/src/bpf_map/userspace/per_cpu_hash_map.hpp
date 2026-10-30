/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#ifndef _BPFTIME_PER_CPU_HASH_MAP_HPP
#define _BPFTIME_PER_CPU_HASH_MAP_HPP
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/smart_ptr/unique_ptr.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <bpf_map/map_common_def.hpp>
#if defined(BPFTIME_ENABLE_MAP_PRODUCTION_AB)
#include <bpf_map/map_production_ab.hpp>
#endif
#include <boost/unordered/unordered_map.hpp>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace bpftime
{

struct per_cpu_bytes_vec_hasher {
	static size_t normal_hash(const uint8_t *data, size_t size)
	{
		using boost::hash_combine;
		if (size == sizeof(uint32_t)) {
			uint32_t key;
			std::memcpy(&key, data, sizeof(key));
			return key;
		}
		size_t seed = 0;
		hash_combine(seed, size);
		for (size_t i = 0; i < size; ++i)
			hash_combine(seed, data[i]);
		return seed;
	}

	size_t operator()(bytes_vec const &vec) const
	{
#if defined(BPFTIME_ENABLE_MAP_PRODUCTION_AB)
		if (map_production_ab_stats_enabled() &&
		    map_production_ab_lookup_active())
			++map_production_ab_stats().hash_calls;
		if (map_production_ab_cached_hash() && vec.size() == 4) {
			uint32_t key;
			std::memcpy(&key, vec.data(), sizeof(key));
			if (key < 1024) {
				static const std::array<size_t, 1024> hashes = [] {
					std::array<size_t, 1024> result{};
					for (uint32_t value = 0; value < result.size(); ++value) {
						uint8_t bytes[sizeof(value)];
						std::memcpy(bytes, &value, sizeof(value));
						result[value] = normal_hash(bytes, sizeof(bytes));
					}
					return result;
				}();
				return hashes[key];
			}
		}
#endif
		return normal_hash(vec.data(), vec.size());
	}
};

struct per_cpu_bytes_vec_equal {
	bool operator()(bytes_vec const &lhs, bytes_vec const &rhs) const
	{
#if defined(BPFTIME_ENABLE_MAP_PRODUCTION_AB)
		if (map_production_ab_stats_enabled() &&
		    map_production_ab_lookup_active())
			++map_production_ab_stats().equal_calls;
#endif
		if (lhs.size() != rhs.size())
			return false;
		if (lhs.size() == sizeof(uint32_t)) {
			uint32_t left, right;
			std::memcpy(&left, lhs.data(), sizeof(left));
			std::memcpy(&right, rhs.data(), sizeof(right));
			return left == right;
		}
		return lhs.empty() ||
		       std::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
	}
};

class per_cpu_hash_map_impl {
	using bi_map_value_ty = std::pair<const bytes_vec, bytes_vec>;
	using bi_map_allocator = boost::interprocess::allocator<
		bi_map_value_ty,
		boost::interprocess::managed_shared_memory::segment_manager>;
	using shm_hash_map =
		boost::unordered_map<bytes_vec, bytes_vec,
				     per_cpu_bytes_vec_hasher,
				     per_cpu_bytes_vec_equal, bi_map_allocator>;
	struct uint32_key_hasher {
		size_t operator()(uint32_t key) const
		{
#if defined(BPFTIME_ENABLE_MAP_PRODUCTION_AB)
			if (map_production_ab_stats_enabled() &&
			    map_production_ab_lookup_active())
				++map_production_ab_stats().hash_calls;
#endif
			return key;
		}
	};
	struct uint32_key_equal {
		bool operator()(uint32_t lhs, uint32_t rhs) const
		{
#if defined(BPFTIME_ENABLE_MAP_PRODUCTION_AB)
			if (map_production_ab_stats_enabled() &&
			    map_production_ab_lookup_active())
				++map_production_ab_stats().equal_calls;
#endif
			return lhs == rhs;
		}
	};
	struct uint32_value_ref {
		boost::interprocess::offset_ptr<uint8_t> data;
		uint32_t slot;
	};
	using bi_uint32_map_value_ty =
		std::pair<const uint32_t, uint32_value_ref>;
	using bi_uint32_map_allocator = boost::interprocess::allocator<
		bi_uint32_map_value_ty,
		boost::interprocess::managed_shared_memory::segment_manager>;
	using shm_uint32_hash_map = boost::unordered_map<
		uint32_t, uint32_value_ref, uint32_key_hasher, uint32_key_equal,
		bi_uint32_map_allocator>;

	using shm_hash_map_vec_allocator = boost::interprocess::allocator<
		shm_hash_map,
		boost::interprocess::managed_shared_memory::segment_manager>;
	using shm_hash_map_vec =
		boost::interprocess::vector<shm_hash_map,
					    shm_hash_map_vec_allocator>;

	using bytes_vec_vec_allocator = boost::interprocess::allocator<
		bytes_vec,
		boost::interprocess::managed_shared_memory::segment_manager>;
	using bytes_vec_vec =
		boost::interprocess::vector<bytes_vec, bytes_vec_vec_allocator>;
	using uint32_vec_allocator = boost::interprocess::allocator<
		uint32_t,
		boost::interprocess::managed_shared_memory::segment_manager>;
	using uint32_vec =
		boost::interprocess::vector<uint32_t, uint32_vec_allocator>;
	using node_type = shm_hash_map::node_type;
	using uint32_node_type = shm_uint32_hash_map::node_type;
	using uint32_node_vec_allocator = boost::interprocess::allocator<
		uint32_node_type,
		boost::interprocess::managed_shared_memory::segment_manager>;
	using uint32_node_vec = boost::interprocess::vector<
		uint32_node_type, uint32_node_vec_allocator>;

	shm_hash_map impl;
	// Four-byte BPF keys are stored inline in the node. The generic map remains
	// the fallback for every other key size.
	shm_uint32_hash_map uint32_impl;
	uint32_t key_size;
	uint32_t value_size;
	int ncpu;
	uint32_t _max_entries;

	bytes_vec value_template;
	bytes_vec_vec key_templates, single_value_templates;
	// Four-byte-key maps keep values in stable, reusable chunks. Hash nodes
	// contain only a pointer and slot number, so erasing a node does not also
	// destroy and deallocate a separately owned bytes_vec.
	bytes_vec_vec uint32_value_chunks;
	uint32_vec uint32_free_next;
	// Keep extracted nodes alive so a later insertion can relink them without
	// returning to the managed shared-memory allocator. This mirrors the
	// preallocated/free-list behavior of kernel hash maps while preserving
	// normal map size and iteration semantics.
	uint32_node_vec uint32_recycled_nodes;
	uint32_t uint32_free_head;
	uint32_t uint32_slot_count;
	uint32_t uint32_slots_per_chunk;
	static std::vector<node_type> &diagnostic_deferred_nodes();
	static std::vector<uint32_node_type> &
	diagnostic_deferred_uint32_nodes();
	// sched_getcpu() can fail (-1) and, with the count fixed at creation,
	// must not index past it.
	bool valid_cpu(int cpu) const
	{
		return cpu >= 0 && cpu < ncpu;
	}
	bool uses_uint32_keys() const
	{
		return key_size == sizeof(uint32_t);
	}
	uint32_value_ref acquire_uint32_value();
	void release_uint32_value(uint32_t slot);
	bool insert_uint32_value(uint32_t key, uint32_value_ref value);
	void recycle_uint32_node(shm_uint32_hash_map::iterator itr);

    public:
	// Only the values are per CPU: the key index, the value slot and node
	// free lists and the key scratch buffers are shared by every thread and
	// process, so every operation runs under the handler's map lock.
	const static bool should_lock = true;

	per_cpu_hash_map_impl(boost::interprocess::managed_shared_memory &memory,
			      uint32_t key_size, uint32_t value_size,
			      uint32_t max_entries);
	per_cpu_hash_map_impl(boost::interprocess::managed_shared_memory &memory,
			      uint32_t key_size, uint32_t value_size,
			      uint32_t max_entries, int ncpu);
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
};
} // namespace bpftime

#endif

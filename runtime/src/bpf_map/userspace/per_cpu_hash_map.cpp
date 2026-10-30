/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#include "bpf_map/map_common_def.hpp"
#include "bpf_map/map_production_ab.hpp"
#include "linux/bpf.h"
#include "spdlog/fmt/bin_to_hex.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <bpf_map/userspace/per_cpu_hash_map.hpp>
#include <cerrno>
#include <cstring>
#include <limits>
#include <unistd.h>
#include "platform_utils.hpp"


namespace bpftime
{
namespace
{
constexpr uint32_t invalid_uint32_value_slot =
	std::numeric_limits<uint32_t>::max();
constexpr uint64_t target_uint32_value_chunk_bytes = 64ULL << 10;
constexpr uint32_t max_uint32_value_slots_per_chunk = 256;
constexpr uint32_t eager_uint32_node_pool_capacity = 1024;

uint32_t choose_uint32_value_slots_per_chunk(uint64_t value_stride)
{
	uint32_t slots = 1;
	while (slots < max_uint32_value_slots_per_chunk &&
	       static_cast<uint64_t>(slots * 2) * value_stride <=
		       target_uint32_value_chunk_bytes)
		slots *= 2;
	return slots;
}
} // namespace

std::vector<per_cpu_hash_map_impl::node_type> &
per_cpu_hash_map_impl::diagnostic_deferred_nodes()
{
	static thread_local std::vector<node_type> nodes;
	if (nodes.capacity() < 1024)
		nodes.reserve(1024);
	return nodes;
}

std::vector<per_cpu_hash_map_impl::uint32_node_type> &
per_cpu_hash_map_impl::diagnostic_deferred_uint32_nodes()
{
	static thread_local std::vector<uint32_node_type> nodes;
	if (nodes.capacity() < 1024)
		nodes.reserve(1024);
	return nodes;
}

per_cpu_hash_map_impl::per_cpu_hash_map_impl(
	boost::interprocess::managed_shared_memory &memory, uint32_t key_size,
	uint32_t value_size, uint32_t max_entries)
	: per_cpu_hash_map_impl(memory, key_size, value_size, max_entries,
				bpftime_num_possible_cpus())
{
}

per_cpu_hash_map_impl::per_cpu_hash_map_impl(
	boost::interprocess::managed_shared_memory &memory, uint32_t key_size,
	uint32_t value_size, uint32_t max_entries, int ncpu)
	: impl(memory.get_segment_manager()),
	  uint32_impl(memory.get_segment_manager()), key_size(key_size),
	  value_size(value_size), ncpu(ncpu), _max_entries(max_entries),
	  value_template(value_size * ncpu, memory.get_segment_manager()),
	  key_templates(memory.get_segment_manager()),
	  single_value_templates(memory.get_segment_manager()),
	  uint32_value_chunks(memory.get_segment_manager()),
	  uint32_free_next(memory.get_segment_manager()),
	  uint32_recycled_nodes(memory.get_segment_manager()),
	  uint32_free_head(invalid_uint32_value_slot), uint32_slot_count(0),
	  uint32_slots_per_chunk(choose_uint32_value_slots_per_chunk(
		  static_cast<uint64_t>(value_size) * ncpu))
{
	SPDLOG_DEBUG(
		"Initializing per cpu hash, key size {}, value size {}, ncpu {}",
		key_size, value_size, ncpu);
	if (uses_uint32_keys())
		uint32_recycled_nodes.reserve(
			std::min(max_entries, eager_uint32_node_pool_capacity));
	for (int i = 0; i < ncpu; i++) {
		if (!uses_uint32_keys()) {
			bytes_vec key_vec(key_size, memory.get_segment_manager());
			key_templates.push_back(key_vec);
		}
		bytes_vec value_vec(value_size, memory.get_segment_manager());
		single_value_templates.push_back(value_vec);
	}
}

per_cpu_hash_map_impl::uint32_value_ref
per_cpu_hash_map_impl::acquire_uint32_value()
{
	uint32_t slot;
	if (uint32_free_head != invalid_uint32_value_slot) {
		slot = uint32_free_head;
		uint32_free_head = uint32_free_next[slot];
	} else {
		if (uint32_slot_count >= _max_entries) {
			errno = E2BIG;
			return { nullptr, invalid_uint32_value_slot };
		}
		slot = uint32_slot_count++;
		if (slot % uint32_slots_per_chunk == 0) {
			const size_t chunk_bytes =
				static_cast<size_t>(value_size) * ncpu *
				uint32_slots_per_chunk;
			bytes_vec chunk(chunk_bytes,
					value_template.get_allocator());
			uint32_value_chunks.push_back(std::move(chunk));
		}
		uint32_free_next.push_back(invalid_uint32_value_slot);
	}

	auto &chunk =
		uint32_value_chunks[slot / uint32_slots_per_chunk];
	uint8_t *data = chunk.data() +
			static_cast<size_t>(slot % uint32_slots_per_chunk) *
				value_size * ncpu;
	std::memset(data, 0, static_cast<size_t>(value_size) * ncpu);
	return { data, slot };
}

void per_cpu_hash_map_impl::release_uint32_value(uint32_t slot)
{
	uint32_free_next[slot] = uint32_free_head;
	uint32_free_head = slot;
}

bool per_cpu_hash_map_impl::insert_uint32_value(uint32_t key,
						 uint32_value_ref value)
{
	if (uint32_recycled_nodes.empty())
		return uint32_impl
			.insert(bi_uint32_map_value_ty(key, value))
			.second;

	auto node = std::move(uint32_recycled_nodes.back());
	uint32_recycled_nodes.pop_back();
	node.key() = key;
	node.mapped() = value;
	auto result = uint32_impl.insert(std::move(node));
	if (!result.inserted)
		uint32_recycled_nodes.push_back(std::move(result.node));
	return result.inserted;
}

void per_cpu_hash_map_impl::recycle_uint32_node(
	shm_uint32_hash_map::iterator itr)
{
	uint32_recycled_nodes.push_back(uint32_impl.extract(itr));
}

__attribute__((flatten)) void *
per_cpu_hash_map_impl::elem_lookup(const void *key)
{
	int cpu = my_sched_getcpu();
	SPDLOG_DEBUG("Run per cpu hash lookup at cpu {}", cpu);
	if (key == nullptr || !valid_cpu(cpu)) {
		errno = ENOENT;
		return nullptr;
	}
	if (get_map_production_ab_mode() ==
	    map_production_ab_mode::percpu_hash_lookup_no_find)
		return &value_template[value_size * cpu];
	const bool collect_stats = map_production_ab_stats_enabled();
	if (collect_stats) {
		map_production_ab_lookup_active() = true;
		++map_production_ab_stats().lookups;
	}
	void *result = nullptr;
	if (uses_uint32_keys()) {
		uint32_t key_value;
		std::memcpy(&key_value, key, sizeof(key_value));
		if (!uint32_impl.empty()) {
			// A lookup only visits one bucket. Local iterators avoid the
			// bucket-group state carried by a shared-memory global iterator.
			const auto bucket = uint32_impl.bucket(key_value);
			const auto equal = uint32_impl.key_eq();
			for (auto itr = uint32_impl.begin(bucket);
			     itr != uint32_impl.end(bucket); ++itr) {
				auto &entry = *itr;
				if (equal(key_value, entry.first)) {
					result = entry.second.data.get() + value_size * cpu;
					break;
				}
			}
		}
	} else {
		bytes_vec &key_vec = this->key_templates[cpu];
		// key_vec is pre-sized in the constructor. Reusing its storage avoids
		// the iterator/range machinery in vector::assign on every helper call.
		std::memcpy(key_vec.data(), key, key_size);
		auto itr = impl.find(key_vec);
		if (itr != impl.end())
			result = itr->second.data.get() + value_size * cpu;
	}
	if (result != nullptr) {
		if (collect_stats) {
			++map_production_ab_stats().hits;
			map_production_ab_lookup_active() = false;
		}
		SPDLOG_TRACE("Exit elem lookup of hash map");
		return result;
	} else {
		if (collect_stats) {
			++map_production_ab_stats().misses;
			map_production_ab_lookup_active() = false;
		}
		SPDLOG_TRACE("Exit elem lookup of hash map");
		errno = ENOENT;
		return nullptr;
	}
}

long per_cpu_hash_map_impl::elem_update(const void *key, const void *value,
					uint64_t flags)
{
	if (get_map_production_ab_mode() ==
	    map_production_ab_mode::percpu_hash_delete_defer_reclaim) {
		diagnostic_deferred_nodes().clear();
		diagnostic_deferred_uint32_nodes().clear();
	}
	if (!check_update_flags(flags))
		return -1;
	int cpu = my_sched_getcpu();
	if (!valid_cpu(cpu)) {
		errno = EINVAL;
		return -1;
	}

	SPDLOG_DEBUG("Run per cpu hash update at cpu {}", cpu);
	const auto mode = get_map_production_ab_mode();
	if (mode == map_production_ab_mode::percpu_hash_update_no_find) {
		// Keep this diagnostic mode measuring both input copies while avoiding
		// any map lookup.
		std::memcpy(single_value_templates[cpu].data(), value,
			    value_size);
		return 0;
	}
	const bool collect_stats = map_production_ab_stats_enabled();
	if (collect_stats) {
		map_production_ab_lookup_active() = true;
		++map_production_ab_stats().lookups;
	}
	uint8_t *existing_value = nullptr;
	if (uses_uint32_keys()) {
		uint32_t key_value;
		std::memcpy(&key_value, key, sizeof(key_value));
		auto itr = uint32_impl.find(key_value);
		if (itr != uint32_impl.end())
			existing_value = itr->second.data.get();
	} else {
		bytes_vec &key_vec = this->key_templates[cpu];
		std::memcpy(key_vec.data(), key, key_size);
		auto itr = impl.find(key_vec);
		if (itr != impl.end())
			existing_value = itr->second.data.get();
	}
	// Kernel percpu htab: BPF_NOEXIST on an existing key fails with EEXIST,
	// BPF_EXIST on a missing key with ENOENT, a new key in a full map with
	// E2BIG. The flags used to be ignored here (only the syscall path checked
	// them), so a racing NOEXIST insert overwrote the winner's value.
	const uint64_t base_flags = flags & 0xFFFFFFFFULL;
	if (base_flags == BPF_NOEXIST && existing_value != nullptr) {
		if (collect_stats)
			map_production_ab_lookup_active() = false;
		errno = EEXIST;
		return -1;
	}
	if (base_flags == BPF_EXIST && existing_value == nullptr) {
		if (collect_stats)
			map_production_ab_lookup_active() = false;
		errno = ENOENT;
		return -1;
	}
	if (existing_value != nullptr) {
		if (collect_stats) {
			++map_production_ab_stats().hits;
			map_production_ab_lookup_active() = false;
		}
		if (mode != map_production_ab_mode::percpu_hash_update_no_copy) {
			std::memcpy(existing_value + cpu * value_size, value,
				    value_size);
		}
	} else {
		if (collect_stats) {
			++map_production_ab_stats().misses;
			map_production_ab_lookup_active() = false;
		}
		if (uses_uint32_keys()) {
			uint32_t key_value;
			std::memcpy(&key_value, key, sizeof(key_value));
			auto value_ref = acquire_uint32_value();
			if (value_ref.data == nullptr)
				return -1;
			std::memcpy(value_ref.data.get() + cpu * value_size,
				    value, value_size);
			if (!insert_uint32_value(key_value, value_ref))
				release_uint32_value(value_ref.slot);
		} else {
			if (impl.size() >= _max_entries) {
				errno = E2BIG;
				return -1;
			}
			auto value_ref = acquire_uint32_value();
			if (value_ref.data == nullptr)
				return -1;
			std::memcpy(value_ref.data.get() + cpu * value_size,
				    value, value_size);
			if (!impl.insert(bi_map_value_ty(key_templates[cpu],
							 value_ref))
				     .second)
				release_uint32_value(value_ref.slot);
		}
	}

	return 0;
}

long per_cpu_hash_map_impl::elem_delete(const void *key)
{
	int cpu = my_sched_getcpu();
	SPDLOG_DEBUG("Run per cpu hash delete at cpu {}", cpu);
	if (!valid_cpu(cpu)) {
		errno = EINVAL;
		return -1;
	}
	if (uses_uint32_keys()) {
		uint32_t key_value;
		std::memcpy(&key_value, key, sizeof(key_value));
		auto itr = uint32_impl.find(key_value);
		if (itr == uint32_impl.end()) {
			errno = ENOENT;
			return -1;
		}
		const uint32_t slot = itr->second.slot;
		if (get_map_production_ab_mode() ==
		    map_production_ab_mode::percpu_hash_delete_defer_reclaim) {
			auto node = uint32_impl.extract(itr);
			diagnostic_deferred_uint32_nodes().push_back(
				std::move(node));
		} else {
			recycle_uint32_node(itr);
		}
		release_uint32_value(slot);
	} else {
		bytes_vec &key_vec = this->key_templates[cpu];
		std::memcpy(key_vec.data(), key, key_size);
		auto itr = impl.find(key_vec);
		if (itr == impl.end()) {
			errno = ENOENT;
			return -1;
		}
		const uint32_t slot = itr->second.slot;
		if (get_map_production_ab_mode() ==
		    map_production_ab_mode::percpu_hash_delete_defer_reclaim) {
			auto node = impl.extract(itr);
			diagnostic_deferred_nodes().push_back(std::move(node));
		} else {
			impl.erase(itr);
		}
		release_uint32_value(slot);
	}
	return 0;
}

int per_cpu_hash_map_impl::map_get_next_key(const void *key, void *next_key)
{
	if (uses_uint32_keys()) {
		if (key == nullptr) {
			auto itr = uint32_impl.begin();
			if (itr == uint32_impl.end()) {
				errno = ENOENT;
				return -1;
			}
			std::memcpy(next_key, &itr->first, sizeof(itr->first));
			return 0;
		}
		uint32_t key_value;
		std::memcpy(&key_value, key, sizeof(key_value));
		auto itr = uint32_impl.find(key_value);
		if (itr == uint32_impl.end())
			return map_get_next_key(nullptr, next_key);
		++itr;
		if (itr == uint32_impl.end()) {
			errno = ENOENT;
			return -1;
		}
		std::memcpy(next_key, &itr->first, sizeof(itr->first));
		return 0;
	}
	if (key == nullptr) {
		// nullptr means the first key
		auto itr = impl.begin();
		if (itr == impl.end()) {
			errno = ENOENT;
			return -1;
		}
		std::copy(itr->first.begin(), itr->first.end(),
			  (uint8_t *)next_key);
		return 0;
	}
	// No need to be allocated at shm. Allocate as a local variable to make
	// it thread safe, since we use sharable lock
	bytes_vec &key_vec = this->key_templates[0];
	std::memcpy(key_vec.data(), key, key_size);

	auto itr = impl.find(key_vec);
	if (itr == impl.end()) {
		// not found, should be refer to the first key
		return map_get_next_key(nullptr, next_key);
	}
	itr++;
	if (itr == impl.end()) {
		// If *key* is the last element, returns -1 and *errno*
		// is set to **ENOENT**.
		errno = ENOENT;
		return -1;
	}
	std::copy(itr->first.begin(), itr->first.end(), (uint8_t *)next_key);
	return 0;
}
void *per_cpu_hash_map_impl::elem_lookup_userspace(const void *key)
{
	if (key == nullptr) {
		errno = ENOENT;
		return nullptr;
	}
	uint8_t *result = nullptr;
	if (uses_uint32_keys()) {
		uint32_t key_value;
		std::memcpy(&key_value, key, sizeof(key_value));
		auto itr = uint32_impl.find(key_value);
		if (itr != uint32_impl.end())
			result = itr->second.data.get();
	} else {
		bytes_vec &key_vec = this->key_templates[0];
		std::memcpy(key_vec.data(), key, key_size);
		auto itr = impl.find(key_vec);
		if (itr != impl.end())
			result = itr->second.data.get();
	}
	if (result != nullptr) {
		SPDLOG_TRACE("Exit elem lookup of hash map");
		if (value_size % 8 == 0)
			return result;
		uint8_t *out = percpu_userspace_buffer(
			percpu_userspace_stride(value_size) * ncpu);
		percpu_values_to_userspace(out, result, value_size, ncpu);
		return out;
	} else {
		SPDLOG_TRACE("Exit elem lookup of hash map");
		errno = ENOENT;
		return nullptr;
	}
}

long per_cpu_hash_map_impl::elem_update_userspace(const void *key,
						  const void *value,
						  uint64_t flags)
{
	if (!check_update_flags(flags))
		return -1;
	bool elem_exists;
	uint32_t key_value = 0;
	shm_uint32_hash_map::iterator uint32_itr;
	shm_hash_map::iterator generic_itr;
	if (uses_uint32_keys()) {
		std::memcpy(&key_value, key, sizeof(key_value));
		uint32_itr = uint32_impl.find(key_value);
		elem_exists = uint32_itr != uint32_impl.end();
	} else {
		bytes_vec &key_vec = this->key_templates[0];
		std::memcpy(key_vec.data(), key, key_size);
		generic_itr = impl.find(key_vec);
		elem_exists = generic_itr != impl.end();
	}
	if (flags == BPF_NOEXIST && elem_exists) {
		errno = EEXIST;
		return -1;
	}
	if (flags == BPF_EXIST && !elem_exists) {
		errno = ENOENT;
		return -1;
	}
	const size_t map_size = uses_uint32_keys() ? uint32_impl.size() :
						    impl.size();
	if (elem_exists == false && map_size == _max_entries) {
		errno = E2BIG;
		return -1;
	}
	if (uses_uint32_keys()) {
		if (elem_exists) {
			percpu_values_from_userspace(
				uint32_itr->second.data.get(),
				(const uint8_t *)value, value_size, ncpu);
		} else {
			auto value_ref = acquire_uint32_value();
			if (value_ref.data == nullptr)
				return -1;
			percpu_values_from_userspace(value_ref.data.get(),
						     (const uint8_t *)value,
						     value_size, ncpu);
			if (!insert_uint32_value(key_value, value_ref)) {
				release_uint32_value(value_ref.slot);
				errno = EEXIST;
				return -1;
			}
		}
	} else {
		if (elem_exists) {
			percpu_values_from_userspace(
				generic_itr->second.data.get(),
				(const uint8_t *)value, value_size, ncpu);
		} else {
			auto value_ref = acquire_uint32_value();
			if (value_ref.data == nullptr)
				return -1;
			percpu_values_from_userspace(value_ref.data.get(),
						     (const uint8_t *)value,
						     value_size, ncpu);
			if (!impl.insert(bi_map_value_ty(key_templates[0],
							 value_ref))
				     .second) {
				release_uint32_value(value_ref.slot);
				errno = EEXIST;
				return -1;
			}
		}
	}
	return 0;
}
long per_cpu_hash_map_impl::elem_delete_userspace(const void *key)
{
	if (uses_uint32_keys()) {
		uint32_t key_value;
		std::memcpy(&key_value, key, sizeof(key_value));
		auto itr = uint32_impl.find(key_value);
		if (itr == uint32_impl.end()) {
			errno = ENOENT;
			return -1;
		}
		const uint32_t slot = itr->second.slot;
		recycle_uint32_node(itr);
		release_uint32_value(slot);
	} else {
		bytes_vec &key_vec = this->key_templates[0];
		std::memcpy(key_vec.data(), key, key_size);
		auto itr = impl.find(key_vec);
		if (itr == impl.end()) {
			errno = ENOENT;
			return -1;
		}
		const uint32_t slot = itr->second.slot;
		impl.erase(itr);
		release_uint32_value(slot);
	}
	return 0;
}

long per_cpu_hash_map_impl::lookup_and_delete_userspace(const void *key,
							void *value)
{
	if (uses_uint32_keys()) {
		uint32_t key_value;
		std::memcpy(&key_value, key, sizeof(key_value));
		auto itr = uint32_impl.find(key_value);
		if (itr == uint32_impl.end()) {
			errno = ENOENT;
			return -1;
		}
		percpu_values_to_userspace((uint8_t *)value,
					   itr->second.data.get(), value_size,
					   ncpu);
		const uint32_t slot = itr->second.slot;
		recycle_uint32_node(itr);
		release_uint32_value(slot);
	} else {
		bytes_vec &key_vec = this->key_templates[0];
		std::memcpy(key_vec.data(), key, key_size);
		auto itr = this->impl.find(key_vec);
		if (itr == impl.end()) {
			errno = ENOENT;
			return -1;
		}
		percpu_values_to_userspace((uint8_t *)value,
					   itr->second.data.get(), value_size,
					   ncpu);
		const uint32_t slot = itr->second.slot;
		impl.erase(itr);
		release_uint32_value(slot);
	}
	return 0;
}
} // namespace bpftime

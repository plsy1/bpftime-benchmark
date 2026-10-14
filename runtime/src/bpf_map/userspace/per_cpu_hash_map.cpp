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
#include <unistd.h>
#include "platform_utils.hpp"


namespace bpftime
{
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
				sysconf(_SC_NPROCESSORS_ONLN))
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
	  single_value_templates(memory.get_segment_manager())
{
	SPDLOG_DEBUG(
		"Initializing per cpu hash, key size {}, value size {}, ncpu {}",
		key_size, value_size, ncpu);
	for (int i = 0; i < ncpu; i++) {
		if (!uses_uint32_keys()) {
			bytes_vec key_vec(key_size, memory.get_segment_manager());
			key_templates.push_back(key_vec);
		}
		bytes_vec value_vec(value_size, memory.get_segment_manager());
		single_value_templates.push_back(value_vec);
	}
}

void *per_cpu_hash_map_impl::elem_lookup(const void *key)
{
	int cpu = my_sched_getcpu();
	SPDLOG_DEBUG("Run per cpu hash lookup at cpu {}", cpu);
	if (key == nullptr) {
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
		auto itr = uint32_impl.find(key_value);
		if (itr != uint32_impl.end())
			result = itr->second.data() + value_size * cpu;
	} else {
		bytes_vec &key_vec = this->key_templates[cpu];
		// key_vec is pre-sized in the constructor. Reusing its storage avoids
		// the iterator/range machinery in vector::assign on every helper call.
		std::memcpy(key_vec.data(), key, key_size);
		auto itr = impl.find(key_vec);
		if (itr != impl.end())
			result = itr->second.data() + value_size * cpu;
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
	SPDLOG_DEBUG("Per cpu update, key {}, value {}", (const char *)key,
		     *(long *)value);

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
	bytes_vec *existing_value = nullptr;
	if (uses_uint32_keys()) {
		uint32_t key_value;
		std::memcpy(&key_value, key, sizeof(key_value));
		auto itr = uint32_impl.find(key_value);
		if (itr != uint32_impl.end())
			existing_value = &itr->second;
	} else {
		bytes_vec &key_vec = this->key_templates[cpu];
		std::memcpy(key_vec.data(), key, key_size);
		auto itr = impl.find(key_vec);
		if (itr != impl.end())
			existing_value = &itr->second;
	}
	if (existing_value != nullptr) {
		if (collect_stats) {
			++map_production_ab_stats().hits;
			map_production_ab_lookup_active() = false;
		}
		if (mode != map_production_ab_mode::percpu_hash_update_no_copy) {
			std::memcpy(existing_value->data() + cpu * value_size, value,
				    value_size);
		}
	} else {
		if (collect_stats) {
			++map_production_ab_stats().misses;
			map_production_ab_lookup_active() = false;
		}
		bytes_vec full_value_vec = this->value_template;
		std::memcpy(full_value_vec.data() + cpu * value_size, value,
			    value_size);

		if (uses_uint32_keys()) {
			uint32_t key_value;
			std::memcpy(&key_value, key, sizeof(key_value));
			uint32_impl.insert(bi_uint32_map_value_ty(key_value,
							       full_value_vec));
		} else {
			impl.insert(bi_map_value_ty(key_templates[cpu],
						    full_value_vec));
		}
	}

	return 0;
}

long per_cpu_hash_map_impl::elem_delete(const void *key)
{
	int cpu = my_sched_getcpu();
	SPDLOG_DEBUG("Run per cpu hash delete at cpu {}", cpu);
	if (uses_uint32_keys()) {
		uint32_t key_value;
		std::memcpy(&key_value, key, sizeof(key_value));
		auto itr = uint32_impl.find(key_value);
		if (itr == uint32_impl.end()) {
			errno = ENOENT;
			return -1;
		}
		if (get_map_production_ab_mode() ==
		    map_production_ab_mode::percpu_hash_delete_defer_reclaim) {
			auto node = uint32_impl.extract(itr);
			diagnostic_deferred_uint32_nodes().push_back(
				std::move(node));
		} else {
			uint32_impl.erase(itr);
		}
	} else {
		bytes_vec &key_vec = this->key_templates[cpu];
		std::memcpy(key_vec.data(), key, key_size);
		auto itr = impl.find(key_vec);
		if (itr == impl.end()) {
			errno = ENOENT;
			return -1;
		}
		if (get_map_production_ab_mode() ==
		    map_production_ab_mode::percpu_hash_delete_defer_reclaim) {
			auto node = impl.extract(itr);
			diagnostic_deferred_nodes().push_back(std::move(node));
		} else {
			impl.erase(itr);
		}
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
	bytes_vec *result = nullptr;
	if (uses_uint32_keys()) {
		uint32_t key_value;
		std::memcpy(&key_value, key, sizeof(key_value));
		auto itr = uint32_impl.find(key_value);
		if (itr != uint32_impl.end())
			result = &itr->second;
	} else {
		bytes_vec &key_vec = this->key_templates[0];
		std::memcpy(key_vec.data(), key, key_size);
		auto itr = impl.find(key_vec);
		if (itr != impl.end())
			result = &itr->second;
	}
	if (result != nullptr) {
		SPDLOG_TRACE("Exit elem lookup of hash map: {}",
			     spdlog::to_hex(result->begin(), result->end()));
		return result->data();
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
	bytes_vec value_vec = this->value_template;
	value_vec.assign((uint8_t *)value,
			 (uint8_t *)value + value_size * ncpu);
	bool elem_exists;
	uint32_t key_value = 0;
	if (uses_uint32_keys()) {
		std::memcpy(&key_value, key, sizeof(key_value));
		elem_exists = uint32_impl.find(key_value) != uint32_impl.end();
	} else {
		bytes_vec &key_vec = this->key_templates[0];
		std::memcpy(key_vec.data(), key, key_size);
		elem_exists = impl.find(key_vec) != impl.end();
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
	if (uses_uint32_keys())
		uint32_impl.insert_or_assign(key_value, value_vec);
	else
		impl.insert_or_assign(key_templates[0], value_vec);
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
		uint32_impl.erase(itr);
	} else {
		bytes_vec &key_vec = this->key_templates[0];
		std::memcpy(key_vec.data(), key, key_size);
		auto itr = impl.find(key_vec);
		if (itr == impl.end()) {
			errno = ENOENT;
			return -1;
		}
		impl.erase(itr);
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
		memcpy(value, itr->second.data(), ncpu * value_size);
		uint32_impl.erase(itr);
	} else {
		bytes_vec &key_vec = this->key_templates[0];
		std::memcpy(key_vec.data(), key, key_size);
		auto itr = this->impl.find(key_vec);
		if (itr == impl.end()) {
			errno = ENOENT;
			return -1;
		}
		memcpy(value, itr->second.data(), ncpu * value_size);
		impl.erase(itr);
	}
	return 0;
}
} // namespace bpftime

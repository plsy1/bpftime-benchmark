/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#ifndef _MAP_COMMON_DEF_HPP
#define _MAP_COMMON_DEF_HPP
#include "bpf_map/map_production_ab.hpp"
#include "spdlog/spdlog.h"
#include <boost/container_hash/hash.hpp>
#include <cinttypes>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <cstdint>
#include <array>
#include <cstring>
#include <functional>
#include "platform_utils.hpp"

namespace bpftime
{

using bytes_vec_allocator = boost::interprocess::allocator<
	uint8_t, boost::interprocess::managed_shared_memory::segment_manager>;
using bytes_vec = boost::interprocess::vector<uint8_t, bytes_vec_allocator>;
using uint64_vec_allocator = boost::interprocess::allocator<
	uint64_t, boost::interprocess::managed_shared_memory::segment_manager>;
using uint64_vec = boost::interprocess::vector<uint64_t, uint64_vec_allocator>;

template <class T>
static inline T ensure_on_current_cpu(std::function<T(int cpu)> func)
{
	return func(my_sched_getcpu());
}

template <class T>
static inline T ensure_on_certain_cpu(int cpu, std::function<T()> func)
{
	static thread_local int currcpu = -1;
	if (currcpu == my_sched_getcpu()) {
		return func(currcpu);
	}
	cpu_set_t orig, set;
	CPU_ZERO(&orig);
	CPU_ZERO(&set);
	sched_getaffinity(0, sizeof(orig), &orig);
	CPU_SET(cpu, &set);
	sched_setaffinity(0, sizeof(set), &set);
	T ret = func();
	sched_setaffinity(0, sizeof(orig), &orig);
	return ret;
}

template <>
inline void ensure_on_certain_cpu(int cpu, std::function<void()> func)
{
	cpu_set_t orig, set;
	CPU_ZERO(&orig);
	CPU_ZERO(&set);
	sched_getaffinity(0, sizeof(orig), &orig);
	CPU_SET(cpu, &set);
	sched_setaffinity(0, sizeof(set), &set);
	func();
	sched_setaffinity(0, sizeof(orig), &orig);
}

struct bytes_vec_hasher {
	static size_t normal_hash(const uint8_t *data, size_t size)
	{
		using boost::hash_combine;
		size_t seed = 0;
		hash_combine(seed, size);
		for (size_t i = 0; i < size; ++i)
			hash_combine(seed, data[i]);
		return seed;
	}

	size_t operator()(bytes_vec const &vec) const
	{
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
		return normal_hash(vec.data(), vec.size());
	}
};

struct bytes_vec_equal {
	bool operator()(bytes_vec const &lhs, bytes_vec const &rhs) const
	{
		if (map_production_ab_stats_enabled() &&
		    map_production_ab_lookup_active())
			++map_production_ab_stats().equal_calls;
		if (map_production_ab_fixed4_equal() && lhs.size() == 4 &&
		    rhs.size() == 4) {
			uint32_t left, right;
			std::memcpy(&left, lhs.data(), sizeof(left));
			std::memcpy(&right, rhs.data(), sizeof(right));
			return left == right;
		}
		return lhs == rhs;
	}
};

struct uint32_hasher {
	size_t operator()(uint32_t const &data) const
	{
		return data;
	}
};

static inline bool check_update_flags(uint64_t flags)
{
	// Allow custom bpftime ops in the high 32 bits; validate only low 32
	// bits
	uint64_t base_flags = flags & 0xFFFFFFFFULL;
	if (base_flags != 0 /*BPF_ANY*/ && base_flags != 1 /*BPF_NOEXIST*/ &&
	    base_flags != 2 /*BPF_EXIST*/) {
		errno = EINVAL;
		return false;
	}
	return true;
}
struct int_hasher {
	size_t operator()(int const &data) const
	{
		return data;
	}
};
} // namespace bpftime

#endif

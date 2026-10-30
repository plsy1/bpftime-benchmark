/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#ifndef _MAP_COMMON_DEF_HPP
#define _MAP_COMMON_DEF_HPP
#include "spdlog/spdlog.h"
#include <boost/container_hash/hash.hpp>
#include <cinttypes>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <cstdint>
#include <functional>
#include <utility>
#include <cstring>
#include <vector>
#include "platform_utils.hpp"

namespace bpftime
{

using bytes_vec_allocator = boost::interprocess::allocator<
	uint8_t, boost::interprocess::managed_shared_memory::segment_manager>;
using bytes_vec = boost::interprocess::vector<uint8_t, bytes_vec_allocator>;
using uint64_vec_allocator = boost::interprocess::allocator<
	uint64_t, boost::interprocess::managed_shared_memory::segment_manager>;
using uint64_vec = boost::interprocess::vector<uint64_t, uint64_vec_allocator>;

template <class T, class Func>
static inline T ensure_on_current_cpu(Func &&func)
{
	// Preserve the callable's concrete type so map helpers can inline it
	// without constructing std::function or allocating for large captures.
	return std::forward<Func>(func)(my_sched_getcpu());
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
	size_t operator()(bytes_vec const &vec) const
	{
		using boost::hash_combine;
		size_t seed = 0;
		hash_combine(seed, vec.size());
		for (auto x : vec)
			hash_combine(seed, x);
		return seed;
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
// Per-CPU values in the bpf() syscall (libbpf, the kernel): one slot per
// possible CPU, each value rounded up to 8 bytes. Maps keep them packed at
// value_size; these convert.
inline size_t percpu_userspace_stride(uint32_t value_size)
{
	return (static_cast<size_t>(value_size) + 7) & ~size_t{ 7 };
}

inline void percpu_values_to_userspace(uint8_t *dst, const uint8_t *src,
				       uint32_t value_size, int ncpu)
{
	const size_t stride = percpu_userspace_stride(value_size);
	for (int cpu = 0; cpu < ncpu; cpu++) {
		std::memcpy(dst + cpu * stride, src + (size_t)cpu * value_size,
			    value_size);
		std::memset(dst + cpu * stride + value_size, 0,
			    stride - value_size);
	}
}

inline void percpu_values_from_userspace(uint8_t *dst, const uint8_t *src,
					 uint32_t value_size, int ncpu)
{
	const size_t stride = percpu_userspace_stride(value_size);
	for (int cpu = 0; cpu < ncpu; cpu++)
		std::memcpy(dst + (size_t)cpu * value_size, src + cpu * stride,
			    value_size);
}

// A per-thread buffer for a lookup result in the syscall layout: the
// syscall copies it out before this thread's next lookup.
inline uint8_t *percpu_userspace_buffer(size_t size)
{
	static thread_local std::vector<uint8_t> buffer;
	if (buffer.size() < size)
		buffer.resize(size);
	return buffer.data();
}

struct int_hasher {
	size_t operator()(int const &data) const
	{
		return data;
	}
};
} // namespace bpftime

#endif

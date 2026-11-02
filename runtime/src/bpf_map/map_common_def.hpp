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
#include <pthread.h>

namespace bpftime
{

// Spin locks of the shared segment (map locks, hash map stripes), taken
// inline on the pthread_spinlock_t word with glibc's generic protocol, which
// aarch64 uses (0 free; an acquire exchange of 1 takes it, a release store of
// 0 gives it back), so that pthread_spin_lock on the same word still
// excludes. Only there: x86-64 glibc keeps its own asm with the opposite
// protocol (pthread_spin_init and unlock store 1, free; lock is `lock decl`,
// held at 0), so an inline exchange never took a lock initialized by
// pthread_spin_init and every map operation spun forever. Other
// architectures and libcs call pthread_spin_lock. Through glibc each
// lock was a PLT call, glibc's function and its out-of-line atomic
// (__aarch64_swp4_acq): 1-4 ns of a 40 ns hash map update on the Jetson.
// The exchange stays __atomic (with -moutline-atomics a call into libgcc
// that uses LSE when the CPU has it): an inline swpa was no faster on the
// Orin, and an ldaxr/stxr loop made an update 5 ns slower.
__attribute__((always_inline)) inline void
bpftime_spin_lock(volatile pthread_spinlock_t *lock)
{
#if defined(__GLIBC__) && defined(__aarch64__)
	int *word = (int *)lock; // glibc: volatile int
	while (__atomic_exchange_n(word, 1, __ATOMIC_ACQUIRE) != 0) {
		while (__atomic_load_n(word, __ATOMIC_RELAXED) != 0)
			__asm__ __volatile__("yield");
	}
#else
	pthread_spin_lock(const_cast<pthread_spinlock_t *>(lock));
#endif
}

__attribute__((always_inline)) inline void
bpftime_spin_unlock(volatile pthread_spinlock_t *lock)
{
#if defined(__GLIBC__) && defined(__aarch64__)
	__atomic_store_n((int *)lock, 0, __ATOMIC_RELEASE);
#else
	pthread_spin_unlock(const_cast<pthread_spinlock_t *>(lock));
#endif
}

// The same lock out of line, for the handler's map lock: inlined there, the
// per-CPU hash helpers grew (and LTO kept bpftime_lock_guard's constructor
// out of line around it), 30 more instructions per update and lookup, while
// the hash map's stripe locks are faster inlined.
__attribute__((noinline)) inline void
bpftime_spin_lock_outlined(volatile pthread_spinlock_t *lock)
{
	bpftime_spin_lock(lock);
}

__attribute__((noinline)) inline void
bpftime_spin_unlock_outlined(volatile pthread_spinlock_t *lock)
{
	bpftime_spin_unlock(lock);
}

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
// possible CPU, in CPU order, each value rounded up to 8 bytes. With holes in
// the possible mask ("0,2-3") slot i holds CPU ids[i]. Maps keep values
// packed at value_size and indexed by CPU number (up to ncpu); these convert.
// A map made for fewer CPUs (tests) leaves the other slots zero.
inline size_t percpu_userspace_stride(uint32_t value_size)
{
	return (static_cast<size_t>(value_size) + 7) & ~size_t{ 7 };
}

inline size_t percpu_userspace_size(uint32_t value_size)
{
	return percpu_userspace_stride(value_size) *
	       bpftime_num_possible_cpus();
}

// The maps' storage is the syscall layout itself: no padding, no holes.
inline bool percpu_userspace_is_packed_storage(uint32_t value_size, int ncpu)
{
	return value_size % 8 == 0 && bpftime_num_possible_cpus() == ncpu &&
	       bpftime_cpu_index_limit() == ncpu;
}

inline void percpu_values_to_userspace(uint8_t *dst, const uint8_t *src,
				       uint32_t value_size, int ncpu)
{
	const size_t stride = percpu_userspace_stride(value_size);
	const auto &ids = bpftime_possible_cpu_ids();
	for (size_t i = 0; i < ids.size(); i++) {
		uint8_t *slot = dst + i * stride;
		size_t copied = 0;
		if (ids[i] < ncpu) {
			std::memcpy(slot, src + (size_t)ids[i] * value_size,
				    value_size);
			copied = value_size;
		}
		std::memset(slot + copied, 0, stride - copied);
	}
}

inline void percpu_values_from_userspace(uint8_t *dst, const uint8_t *src,
					 uint32_t value_size, int ncpu)
{
	const size_t stride = percpu_userspace_stride(value_size);
	const auto &ids = bpftime_possible_cpu_ids();
	for (size_t i = 0; i < ids.size(); i++)
		if (ids[i] < ncpu)
			std::memcpy(dst + (size_t)ids[i] * value_size,
				    src + i * stride, value_size);
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

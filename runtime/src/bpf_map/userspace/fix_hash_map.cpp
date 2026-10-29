/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#include "linux/bpf.h"
#include "spdlog/spdlog.h"
#include <bpf_map/userspace/fix_hash_map.hpp>
#include <algorithm>
#include <cerrno>
#include <functional>
#include <unistd.h>
#include <unordered_map>

namespace bpftime
{

fix_size_hash_map_impl::fix_size_hash_map_impl(managed_shared_memory &memory,
					       size_t num_buckets,
					       size_t key_size,
					       size_t value_size)
	: map_impl(memory, num_buckets, key_size, value_size),
	  _key_size(key_size), _value_size(value_size),
	  _num_buckets(map_impl.bucket_count())
{
	pthread_spin_init(&writer_lock, PTHREAD_PROCESS_SHARED);
}

namespace
{
class writer_guard {
	pthread_spinlock_t &lock;

    public:
	explicit writer_guard(pthread_spinlock_t &lock) : lock(lock)
	{
		pthread_spin_lock(&lock);
	}
	~writer_guard()
	{
		pthread_spin_unlock(&lock);
	}
	writer_guard(const writer_guard &) = delete;
	writer_guard &operator=(const writer_guard &) = delete;
};
} // namespace

void *fix_size_hash_map_impl::elem_lookup(const void *key)
{
	SPDLOG_TRACE("Peform elem lookup of hash map");
	return map_impl.elem_lookup(key);
}

long fix_size_hash_map_impl::elem_update(const void *key, const void *value,
					 uint64_t flags)
{
	// Kernel htab semantics. BPF_F_LOCK is ignored as before.
	const uint64_t mode = flags & ~static_cast<uint64_t>(BPF_F_LOCK);
	if (mode > BPF_EXIST) {
		errno = EINVAL;
		return -1;
	}
	writer_guard guard(writer_lock);
	if (mode != BPF_ANY) {
		// Under the writer lock, so the check and the update are
		// atomic. BPF_EXIST matters to lookup-then-update counters: a
		// key deleted in between must not come back with its old count.
		const bool exists = map_impl.elem_lookup(key) != nullptr;
		if (mode == BPF_NOEXIST && exists) {
			errno = EEXIST;
			return -1;
		}
		if (mode == BPF_EXIST && !exists) {
			errno = ENOENT;
			return -1;
		}
	}
	if (!map_impl.elem_update(key, value)) {
		errno = E2BIG; // full: max_entries reached
		return -1;
	}
	return 0;
}

long fix_size_hash_map_impl::elem_delete(const void *key)
{
	writer_guard guard(writer_lock);
	if (!map_impl.elem_delete(key)) {
		errno = ENOENT;
		return -1;
	}
	return 0;
}

int fix_size_hash_map_impl::map_get_next_key(const void *key, void *next_key)
{
	SPDLOG_TRACE("Peform map get next key of hash map");
	if (next_key == nullptr) {
		errno = EINVAL;
		return -1;
	}
	writer_guard guard(writer_lock);
	return get_next_key_locked(key, next_key);
}

int fix_size_hash_map_impl::get_next_key_locked(const void *key,
						void *next_key)
{
	if (key == nullptr) {
		// get the first key
		for (size_t i = 0; i < _num_buckets; i++) {
			if (map_impl.is_empty(i)) {
				continue;
			}
			memcpy(next_key, map_impl.get_key(i), _key_size);
			return 0;
		}
		errno = ENOENT;
		// no key found
		return -1;
	}
	// get the next key
	void *value_ptr = elem_lookup(key);
	if (value_ptr == nullptr) {
		return get_next_key_locked(nullptr, next_key);
	}
	// get_next_key
	size_t index = map_impl.get_index_of_value(value_ptr);
	for (size_t i = index + 1; i < _num_buckets; i++) {
		if (map_impl.is_empty(i)) {
			continue;
		}
		memcpy(next_key, map_impl.get_key(i), _key_size);
		return 0;
	}
	// if this is the last key
	errno = ENOENT;
	return -1;
}

} // namespace bpftime

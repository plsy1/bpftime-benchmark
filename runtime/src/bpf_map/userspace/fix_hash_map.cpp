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
}

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
	// The existence check and the update are one locked operation.
	// BPF_EXIST matters to lookup-then-update counters: a key deleted in
	// between must not come back with its old count.
	using update_mode = bpftime_hash_map::update_mode;
	using update_result = bpftime_hash_map::update_result;
	switch (map_impl.update(key, value,
				mode == BPF_NOEXIST ? update_mode::no_exist :
				mode == BPF_EXIST   ? update_mode::exist :
						      update_mode::any)) {
	case update_result::ok:
		return 0;
	case update_result::exists:
		errno = EEXIST;
		return -1;
	case update_result::missing:
		errno = ENOENT;
		return -1;
	case update_result::full:
		break;
	}
	errno = E2BIG; // full: max_entries reached
	return -1;
}

long fix_size_hash_map_impl::elem_delete(const void *key)
{
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
	if (!map_impl.next_key(key, next_key)) {
		errno = ENOENT; // empty map or the last key
		return -1;
	}
	return 0;
}

} // namespace bpftime

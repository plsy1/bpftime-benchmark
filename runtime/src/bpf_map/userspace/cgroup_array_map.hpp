/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026, eunomia-bpf org
 * All rights reserved.
 */
#ifndef _BPFTIME_CGROUP_ARRAY_MAP_HPP
#define _BPFTIME_CGROUP_ARRAY_MAP_HPP
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <climits>
#include <cstddef>
#include <cstdint>

namespace bpftime
{

// BPF_MAP_TYPE_CGROUP_ARRAY. The loader stores a cgroup directory fd, which
// means nothing in the traced process, so each slot keeps the cgroup v2 path
// (relative to the cgroup namespace root) resolved when the fd is stored.
// Loader and traced process must share the cgroup namespace.
class cgroup_array_map_impl {
	struct slot {
		bool used;
		char path[PATH_MAX];
	};
	using slot_allocator = boost::interprocess::allocator<
		slot, boost::interprocess::managed_shared_memory::segment_manager>;
	boost::interprocess::vector<slot, slot_allocator> slots;
	uint32_t _max_entries;

    public:
	const static bool should_lock = true;
	cgroup_array_map_impl(boost::interprocess::managed_shared_memory &memory,
			      uint32_t key_size, uint32_t value_size,
			      uint32_t max_entries);

	// Not readable, as in the kernel (EOPNOTSUPP).
	void *elem_lookup(const void *key);
	// value: a cgroup v2 directory fd of the calling process. BPF_ANY only.
	long elem_update(const void *key, const void *value, uint64_t flags);
	long elem_delete(const void *key);
	int map_get_next_key(const void *key, void *next_key);

	// Copies the slot's cgroup path: 0, -E2BIG (index out of range) or
	// -EAGAIN (empty slot). Callers hold the map lock only for this copy.
	long slot_path(uint32_t index, char *buf, size_t size) const;
	// bpf_current_task_under_cgroup: 1 if the calling thread is in the
	// slot's cgroup or a descendant, 0 if not, -E2BIG for an index out of
	// range, -EAGAIN for an empty slot (kernel return values).
	long current_task_under_cgroup(uint32_t index) const;
};

// cgroup v2 path of the calling thread from /proc/thread-self/cgroup, e.g.
// "/user.slice/session-1.scope". False if there is no cgroup v2 entry.
bool current_thread_cgroup_path(char *buf, size_t size);
// Whether `path` is `ancestor` or below it (cgroup v2 paths).
bool cgroup_path_is_under(const char *path, const char *ancestor);
// 1 if the calling thread's cgroup is `ancestor` or below it, else 0.
long cgroup_task_under_path(const char *ancestor);
// bpf_get_current_cgroup_id: the cgroup v2 id of the calling thread, which
// is the inode number of its cgroup directory; 0 if unavailable.
uint64_t current_thread_cgroup_id();

} // namespace bpftime
#endif

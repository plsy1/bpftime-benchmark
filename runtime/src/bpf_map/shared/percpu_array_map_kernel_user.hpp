/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#ifndef _BPFTIME_PERCPU_ARRAY_MAP_KERNEL_USER_HPP
#define _BPFTIME_PERCPU_ARRAY_MAP_KERNEL_USER_HPP
#include <cstdint>
#include <bpf_map/map_common_def.hpp>
namespace bpftime
{

// A percpu array map shared with a kernel map: every operation is a syscall on
// the kernel map, through this process's own descriptor. Programs see the
// calling CPU's value (a copy in a per-thread buffer); their updates rewrite
// that CPU's value only. Userspace (syscall) access uses the kernel layout:
// one value per possible CPU, each rounded up to 8 bytes.
class percpu_array_map_kernel_user_impl {
	int kernel_map_id;

	// Values of all possible CPUs, kernel layout, in a per-thread buffer.
	uint8_t *read_all_cpus(const void *key, uint32_t *stride);

    public:
	const static bool should_lock = false;
	void *elem_lookup(const void *key);

	long elem_update(const void *key, const void *value, uint64_t flags);

	void *elem_lookup_userspace(const void *key);

	long elem_update_userspace(const void *key, const void *value,
				   uint64_t flags);

	long elem_delete(const void *key);

	int map_get_next_key(const void *key, void *next_key);
	percpu_array_map_kernel_user_impl(
		boost::interprocess::managed_shared_memory &memory,
		int kernel_map_id);
	~percpu_array_map_kernel_user_impl();
};
} // namespace bpftime
#endif

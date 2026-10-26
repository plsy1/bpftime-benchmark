/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#include "spdlog/spdlog.h"
#include <bpf_map/shared/hash_map_kernel_user.hpp>
#include <bpf_map/shared/kernel_map_local.hpp>
#include <unistd.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

namespace bpftime
{

hash_map_kernel_user_impl::hash_map_kernel_user_impl(managed_shared_memory &,
						     int km_id)
	: kernel_map_id(km_id)
{
}

void *hash_map_kernel_user_impl::elem_lookup(const void *key)
{
	SPDLOG_TRACE("Peform elem lookup of hash map");
	const kernel_map_local *map = kernel_map_of_this_process(kernel_map_id);
	if (map == nullptr)
		return nullptr;
	void *value = kernel_map_thread_buffer(kernel_map_id, map->value_size);
	if (bpf_map_lookup_elem(map->fd, key, value) < 0)
		return nullptr;
	return value;
}

long hash_map_kernel_user_impl::elem_update(const void *key, const void *value,
					    uint64_t flags)
{
	SPDLOG_TRACE("Peform elem update of hash map");
	const kernel_map_local *map = kernel_map_of_this_process(kernel_map_id);
	if (map == nullptr)
		return -1;
	return bpf_map_update_elem(map->fd, key, value, flags);
}

long hash_map_kernel_user_impl::elem_delete(const void *key)
{
	SPDLOG_TRACE("Peform elem delete of hash map");
	const kernel_map_local *map = kernel_map_of_this_process(kernel_map_id);
	if (map == nullptr)
		return -1;
	return bpf_map_delete_elem(map->fd, key);
}

int hash_map_kernel_user_impl::map_get_next_key(const void *key, void *next_key)
{
	SPDLOG_TRACE("Peform get next key of hash map");
	const kernel_map_local *map = kernel_map_of_this_process(kernel_map_id);
	if (map == nullptr)
		return -1;
	return bpf_map_get_next_key(map->fd, key, next_key);
}

hash_map_kernel_user_impl::~hash_map_kernel_user_impl()
{
}

} // namespace bpftime

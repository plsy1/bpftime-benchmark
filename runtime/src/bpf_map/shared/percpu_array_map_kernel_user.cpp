/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#include "bpf/bpf.h"
#include "bpf/libbpf.h"
#include "platform_utils.hpp"
#include "spdlog/spdlog.h"
#include <bpf_map/shared/kernel_map_local.hpp>
#include <bpf_map/shared/percpu_array_map_kernel_user.hpp>
#include <cerrno>
#include <cstring>
#include <linux/bpf.h>
namespace bpftime
{
namespace
{
int possible_cpus()
{
	static const int n = [] {
		int v = libbpf_num_possible_cpus();
		return v > 0 ? v : 1;
	}();
	return n;
}

// The kernel's per-CPU value stride (round_up(value_size, 8)).
uint32_t percpu_stride(uint32_t value_size)
{
	return (value_size + 7) & ~7u;
}

int current_cpu()
{
	int cpu = my_sched_getcpu();
	return cpu >= 0 && cpu < possible_cpus() ? cpu : 0;
}
} // namespace

uint8_t *percpu_array_map_kernel_user_impl::read_all_cpus(const void *key,
							   uint32_t *stride)
{
	const kernel_map_local *map = kernel_map_of_this_process(kernel_map_id);
	if (map == nullptr)
		return nullptr;
	*stride = percpu_stride(map->value_size);
	auto *values = (uint8_t *)kernel_map_thread_buffer(
		kernel_map_id, (size_t)*stride * possible_cpus());
	if (int err = bpf_map_lookup_elem(map->fd, key, values); err < 0) {
		SPDLOG_DEBUG("Lookup of kernel percpu array {} failed: {}",
			     kernel_map_id, err);
		return nullptr;
	}
	return values;
}

void *percpu_array_map_kernel_user_impl::elem_lookup(const void *key)
{
	uint32_t stride;
	uint8_t *values = read_all_cpus(key, &stride);
	return values ? values + (size_t)current_cpu() * stride : nullptr;
}

void *percpu_array_map_kernel_user_impl::elem_lookup_userspace(const void *key)
{
	uint32_t stride;
	return read_all_cpus(key, &stride);
}

long percpu_array_map_kernel_user_impl::elem_update(const void *key,
						    const void *value,
						    uint64_t flags)
{
	// A program updates its CPU's value: read the others, replace this
	// one, write all back (not atomic against other writers of the
	// element, unlike the kernel's in-place update).
	uint32_t stride;
	uint8_t *values = read_all_cpus(key, &stride);
	if (values == nullptr)
		return -1;
	const kernel_map_local *map = kernel_map_of_this_process(kernel_map_id);
	memcpy(values + (size_t)current_cpu() * stride, value, map->value_size);
	return bpf_map_update_elem(map->fd, key, values, flags);
}

long percpu_array_map_kernel_user_impl::elem_update_userspace(
	const void *key, const void *value, uint64_t flags)
{
	const kernel_map_local *map = kernel_map_of_this_process(kernel_map_id);
	if (map == nullptr)
		return -1;
	return bpf_map_update_elem(map->fd, key, value, flags);
}

long percpu_array_map_kernel_user_impl::elem_delete(const void *key)
{
	const kernel_map_local *map = kernel_map_of_this_process(kernel_map_id);
	if (map == nullptr)
		return -1;
	return bpf_map_delete_elem(map->fd, key);
}

int percpu_array_map_kernel_user_impl::map_get_next_key(const void *key,
							void *next_key)
{
	const kernel_map_local *map = kernel_map_of_this_process(kernel_map_id);
	if (map == nullptr)
		return -1;
	return bpf_map_get_next_key(map->fd, key, next_key);
}
percpu_array_map_kernel_user_impl::percpu_array_map_kernel_user_impl(
	boost::interprocess::managed_shared_memory &, int kernel_map_id)
	: kernel_map_id(kernel_map_id)
{
}
percpu_array_map_kernel_user_impl::~percpu_array_map_kernel_user_impl()
{
}
} // namespace bpftime

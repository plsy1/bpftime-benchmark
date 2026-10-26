/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#include "spdlog/spdlog.h"
#include <bpf/bpf.h>
#include <bpf_map/shared/kernel_map_local.hpp>
#include <mutex>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace bpftime
{
namespace
{
std::mutex local_maps_lock;
// Never shrinks: entries are referenced by pointer for the process lifetime.
std::unordered_map<int, kernel_map_local> &local_maps()
{
	static auto *maps = new std::unordered_map<int, kernel_map_local>();
	return *maps;
}
} // namespace

const kernel_map_local *kernel_map_of_this_process(int kernel_map_id)
{
	// The per-thread cache keeps the lock off the per-event path.
	static thread_local std::unordered_map<int, const kernel_map_local *>
		cache;
	if (auto it = cache.find(kernel_map_id); it != cache.end())
		return it->second;
	std::lock_guard<std::mutex> guard(local_maps_lock);
	auto &maps = local_maps();
	auto it = maps.find(kernel_map_id);
	if (it == maps.end()) {
		int fd = bpf_map_get_fd_by_id(kernel_map_id);
		if (fd < 0) {
			SPDLOG_ERROR("Failed to open kernel map id {}: {}",
				     kernel_map_id, fd);
			return nullptr;
		}
		bpf_map_info info = {};
		uint32_t len = sizeof(info);
		if (bpf_obj_get_info_by_fd(fd, &info, &len) < 0) {
			SPDLOG_ERROR("Failed to get info of kernel map id {}",
				     kernel_map_id);
			close(fd);
			return nullptr;
		}
		kernel_map_local local;
		local.fd = fd;
		local.key_size = info.key_size;
		local.value_size = info.value_size;
		local.max_entries = info.max_entries;
		it = maps.emplace(kernel_map_id, local).first;
	}
	cache.emplace(kernel_map_id, &it->second);
	return &it->second;
}

void *kernel_map_thread_buffer(int kernel_map_id, size_t size)
{
	static thread_local std::unordered_map<int, std::vector<uint64_t> >
		buffers;
	auto &buf = buffers[kernel_map_id];
	const size_t words = (size + sizeof(uint64_t) - 1) / sizeof(uint64_t);
	if (buf.size() < words)
		buf.resize(words);
	return buf.data();
}
} // namespace bpftime

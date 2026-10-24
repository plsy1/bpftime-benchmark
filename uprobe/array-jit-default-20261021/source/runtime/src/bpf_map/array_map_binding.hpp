/* SPDX-License-Identifier: MIT */
#ifndef BPFTIME_ARRAY_MAP_BINDING_HPP
#define BPFTIME_ARRAY_MAP_BINDING_HPP

#include <handler/map_handler.hpp>
#include <boost/interprocess/sync/interprocess_recursive_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <memory>
#include <atomic>
#include <cstdint>
#include <optional>

namespace bpftime {
namespace detail {
extern std::atomic<uint64_t> array_binding_generation;
bool initialize_array_binding_fork_guard();
}

// A separately named object leaves all shared handler/map layouts unchanged.
// Acquisition and handler replacement must use this same cold-path mutex.
inline auto &map_binding_mutex(boost::interprocess::managed_shared_memory &mem)
{
	return *mem.find_or_construct<boost::interprocess::interprocess_recursive_mutex>(
		"bpftime_map_binding_mutex_v1")();
}
using map_binding_guard = boost::interprocess::scoped_lock<
	boost::interprocess::interprocess_recursive_mutex>;

class handler_manager;

// Own both an implementation reference and the exact local mapping used by
// generated code. FD close/reuse, reset and unlink cannot change this identity.
class array_map_binding {
	std::shared_ptr<boost::interprocess::managed_shared_memory> memory;
	std::unique_ptr<bpf_map_handler> handler;
	const uint64_t owner_generation;

	array_map_binding(std::shared_ptr<boost::interprocess::managed_shared_memory> mem,
		const bpf_map_handler &source);
public:
	~array_map_binding();
	array_map_binding(const array_map_binding &) = delete;
	array_map_binding &operator=(const array_map_binding &) = delete;
	static std::shared_ptr<array_map_binding> acquire(
		std::shared_ptr<boost::interprocess::managed_shared_memory> memory,
		const handler_manager *manager, int fd, std::optional<bool> per_cpu = false);
	bool is_per_cpu() const
	{
		return handler->type == bpf_map_type::BPF_MAP_TYPE_PERCPU_ARRAY;
	}
	per_cpu_array_map_impl *percpu_implementation() const
	{
		return static_cast<per_cpu_array_map_impl *>(handler->map_impl_ptr.get());
	}
	uint32_t value_size() const
	{
		return is_per_cpu() ? percpu_implementation()->get_value_size() : implementation()->get_value_size();
	}
	uint32_t max_entries() const
	{
		return is_per_cpu() ? percpu_implementation()->get_max_entries() : implementation()->get_max_entries();
	}
	uint32_t cpu_count() const
	{
		return is_per_cpu() ? percpu_implementation()->get_cpu_count() : 1;
	}
	void *raw_data() const
	{
		return is_per_cpu() ? percpu_implementation()->get_raw_data() : implementation()->get_raw_data();
	}
	void *lookup(const void *key) const
	{
		return is_per_cpu() ? percpu_implementation()->lookup_bound(key) : implementation()->elem_lookup(key);
	}
	long update(const void *key, const void *value, uint64_t flags) const
	{
		return handler->map_update_elem(key, value, flags, false);
	}
	long erase(const void *key) const
	{
		return handler->map_delete_elem(key, false);
	}
	array_map_impl *implementation() const
	{
		return *handler->try_get_array_map_impl();
	}
	bool belongs_to_this_process() const
	{
		return owner_generation == detail::array_binding_generation.load(std::memory_order_relaxed);
	}
};
} // namespace bpftime
#endif

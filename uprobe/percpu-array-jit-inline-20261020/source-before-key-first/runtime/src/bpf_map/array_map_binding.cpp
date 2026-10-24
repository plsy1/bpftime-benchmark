/* SPDX-License-Identifier: MIT */
#include "array_map_binding.hpp"
#include <handler/handler_manager.hpp>
#include <mutex>
#include <pthread.h>
#include <unistd.h>

namespace bpftime {
namespace detail {
std::atomic<uint64_t> array_binding_generation{0};
static_assert(std::atomic<uint64_t>::is_always_lock_free);
bool initialize_array_binding_fork_guard()
{
	static std::once_flag once;
	static int status = -1;
	try {
		std::call_once(once, [] {
			status = pthread_atfork(nullptr, nullptr, [] {
				// Only the child thread exists here; lock-free load/store
				// needs no allocator, mutex, syscall or PID-namespace assumption.
				auto generation = array_binding_generation.load(std::memory_order_relaxed);
				array_binding_generation.store(generation + 1, std::memory_order_relaxed);
			});
		});
	} catch (...) {
		return false;
	}
	return status == 0;
}
}

array_map_binding::array_map_binding(
	std::shared_ptr<boost::interprocess::managed_shared_memory> mem,
	const bpf_map_handler &source) : memory(std::move(mem)),
	owner_generation(detail::array_binding_generation.load(std::memory_order_relaxed))
{
	bpf_map_attr attr{};
	attr.type = static_cast<int>(source.type);
	attr.key_size = sizeof(uint32_t);
	if (source.type == bpf_map_type::BPF_MAP_TYPE_PERCPU_ARRAY) {
		auto *impl = static_cast<per_cpu_array_map_impl *>(source.map_impl_ptr.get());
		attr.value_size = impl->get_value_size();
		attr.max_ents = impl->get_max_entries();
	} else {
		auto *impl = *source.try_get_array_map_impl();
		attr.value_size = impl->get_value_size();
		attr.max_ents = impl->get_max_entries();
	}
	handler = std::make_unique<bpf_map_handler>(0,
		"", *memory, attr); // Empty snapshot name owns no shared allocation.
	handler->share_map_impl_from(source);
	source.inc_map_refcount();
}

array_map_binding::~array_map_binding()
{
	if (!belongs_to_this_process()) {
		// fork copied this local ownership graph, not its shared refcount.
		// Do not decrement/free the parent's implementation or name storage.
		handler->map_impl_ptr = nullptr;
		handler->map_refcnt_ptr = nullptr;
		handler.reset();
		return;
	}
	// There can be several bindings and duplicated descriptors. Existing map
	// reference counting destroys the backing only after the last owner leaves.
	map_binding_guard guard(map_binding_mutex(*memory));
	handler->map_free(*memory);
	handler.reset(); // Its allocator still needs the retained mapping.
}

std::shared_ptr<array_map_binding> array_map_binding::acquire(
	std::shared_ptr<boost::interprocess::managed_shared_memory> memory,
	const handler_manager *manager, int fd, bool per_cpu)
{
	if (!memory || !manager)
		return {};
	if (!detail::initialize_array_binding_fork_guard())
		return {};
	map_binding_guard guard(map_binding_mutex(*memory));
	if (fd < 0 || static_cast<size_t>(fd) >= manager->size())
		return {};
	const auto *map = std::get_if<bpf_map_handler>(&manager->get_handler(fd));
	const auto expected = per_cpu ? bpf_map_type::BPF_MAP_TYPE_PERCPU_ARRAY : bpf_map_type::BPF_MAP_TYPE_ARRAY;
	if (!map || map->type != expected ||
	    map->get_key_size() != sizeof(uint32_t) || !map->can_share_map_impl())
		return {};
	return std::shared_ptr<array_map_binding>(new array_map_binding(
		std::move(memory), *map));
}
} // namespace bpftime

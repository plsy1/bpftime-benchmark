/* SPDX-License-Identifier: MIT */
#include "array_map_binding.hpp"
#include "fork_handlers.hpp"
#include <handler/handler_manager.hpp>
#include <mutex>
#include <unordered_set>
#include <pthread.h>
#include <unistd.h>

namespace bpftime {
namespace detail {
namespace {
// Process-local list of live bindings. fork() holds the lock from prepare to
// the parent/child handlers, so no binding is created or released meanwhile.
pthread_mutex_t live_bindings_lock = PTHREAD_MUTEX_INITIALIZER;
std::unordered_set<const array_map_binding *> &live_bindings()
{
	static auto *bindings = new std::unordered_set<const array_map_binding *>;
	return *bindings;
}
struct live_bindings_guard {
	live_bindings_guard() { pthread_mutex_lock(&live_bindings_lock); }
	~live_bindings_guard() { pthread_mutex_unlock(&live_bindings_lock); }
};
}
// The child inherits a copy of every binding and will release it, so it
// needs its own shared reference. Taking it before the fork leaves no window
// in which the parent could free the map. A failed fork, or a child that
// execs or _exits, leaks them. Registered while the runtime loads: fork
// handlers are registered only from static initializers (fork_handlers.cpp).
const int array_binding_atfork_status = bpftime::register_fork_handlers(
	[] {
		pthread_mutex_lock(&live_bindings_lock);
		for (auto *binding : live_bindings())
			binding->pin_for_fork();
	},
	[] { pthread_mutex_unlock(&live_bindings_lock); },
	[] {
		// Only this thread exists; the lock owner was the parent's.
		pthread_mutex_init(&live_bindings_lock, nullptr);
	});
bool initialize_array_binding_fork_handlers()
{
	return array_binding_atfork_status == 0;
}
}

array_map_binding::array_map_binding(
	std::shared_ptr<boost::interprocess::managed_shared_memory> mem,
	const bpf_map_handler &source) : memory(std::move(mem))
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
	detail::live_bindings_guard guard;
	source.inc_map_refcount();
	detail::live_bindings().insert(this);
}

array_map_binding::~array_map_binding()
{
	{
		detail::live_bindings_guard guard;
		detail::live_bindings().erase(this);
	}
	// There can be several bindings and duplicated descriptors. Existing map
	// reference counting destroys the backing only after the last owner leaves.
	map_binding_guard guard(map_binding_mutex(*memory));
	handler->map_free(*memory);
	handler.reset(); // Its allocator still needs the retained mapping.
}

std::shared_ptr<array_map_binding> array_map_binding::acquire(
	std::shared_ptr<boost::interprocess::managed_shared_memory> memory,
	const handler_manager *manager, int fd, std::optional<bool> per_cpu)
{
	if (!memory || !manager)
		return {};
	if (!detail::initialize_array_binding_fork_handlers())
		return {};
	map_binding_guard guard(map_binding_mutex(*memory));
	if (fd < 0 || static_cast<size_t>(fd) >= manager->size())
		return {};
	const auto *map = std::get_if<bpf_map_handler>(&manager->get_handler(fd));
	if (!map || (map->type != bpf_map_type::BPF_MAP_TYPE_ARRAY &&
	             map->type != bpf_map_type::BPF_MAP_TYPE_PERCPU_ARRAY) ||
	    (per_cpu && (*per_cpu != (map->type == bpf_map_type::BPF_MAP_TYPE_PERCPU_ARRAY))) ||
	    map->get_key_size() != sizeof(uint32_t) || !map->can_share_map_impl())
		return {};
	return std::shared_ptr<array_map_binding>(new array_map_binding(
		std::move(memory), *map));
}
} // namespace bpftime

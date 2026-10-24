/* SPDX-License-Identifier: MIT */
#include "array_map_binding.hpp"
#include <handler/handler_manager.hpp>

namespace bpftime {
array_map_binding::array_map_binding(
	std::shared_ptr<boost::interprocess::managed_shared_memory> mem,
	const bpf_map_handler &source) : memory(std::move(mem))
{
	auto *impl = *source.try_get_array_map_impl();
	bpf_map_attr attr{};
	attr.type = static_cast<int>(bpf_map_type::BPF_MAP_TYPE_ARRAY);
	attr.key_size = sizeof(uint32_t);
	attr.value_size = impl->get_value_size();
	attr.max_ents = impl->get_max_entries();
	handler = std::make_unique<bpf_map_handler>(0,
		source.name.c_str(), *memory, attr);
	handler->share_map_impl_from(source);
	source.inc_map_refcount();
}

array_map_binding::~array_map_binding()
{
	// There can be several bindings and duplicated descriptors. Existing map
	// reference counting destroys the backing only after the last owner leaves.
	map_binding_guard guard(map_binding_mutex(*memory));
	handler->map_free(*memory);
	handler.reset(); // Its allocator still needs the retained mapping.
}

std::shared_ptr<array_map_binding> array_map_binding::acquire(
	std::shared_ptr<boost::interprocess::managed_shared_memory> memory,
	const handler_manager *manager, int fd)
{
	if (!memory || !manager)
		return {};
	map_binding_guard guard(map_binding_mutex(*memory));
	if (fd < 0 || static_cast<size_t>(fd) >= manager->size())
		return {};
	const auto *map = std::get_if<bpf_map_handler>(&manager->get_handler(fd));
	if (!map || map->type != bpf_map_type::BPF_MAP_TYPE_ARRAY ||
	    map->get_key_size() != sizeof(uint32_t) || !map->can_share_map_impl())
		return {};
	return std::shared_ptr<array_map_binding>(new array_map_binding(
		std::move(memory), *map));
}
} // namespace bpftime

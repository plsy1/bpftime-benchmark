#include "bpf_map/map_common_def.hpp"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <boost/container_hash/hash_fwd.hpp>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>

#include "stack_trace_map.hpp"
using namespace bpftime;

stack_trace_map_impl::stack_trace_map_impl(
	boost::interprocess::managed_shared_memory &memory, uint32_t key_size,
	uint32_t value_size, uint32_t max_entries)
	: max_entries(max_entries),
	  data(max_entries, uint32_hasher(), std::equal_to<uint32_t>(),
	       stack_trace_map::map_allocator(memory.get_segment_manager())),
	  key_buf(value_size / 8, memory.get_segment_manager())
{
	if (key_size != 4) {
		SPDLOG_ERROR("Key size of stack trace map must be 4");
		throw std::runtime_error(
			"Key size of stack trace map must be 4");
	}
	if (value_size % 8 != 0) {
		SPDLOG_ERROR(
			"value_size of stack_trace_map must be a multiple of 8, unexpected: {}",
			value_size);
		throw std::runtime_error("Invalid value_size");
	}
	max_stack_entries = value_size / 8;
}

void *stack_trace_map_impl::elem_lookup(const void *key)
{
	if (key == nullptr) {
		errno = EINVAL;
		SPDLOG_ERROR(
			"You can't lookup a stack trace map with NULL key");
		return nullptr;
	}
	uint32_t index = *(int *)key;
	SPDLOG_DEBUG("Performing stack trace lookup with key={}", index);

	auto itr = this->data.find(index);
	if (itr == data.end()) {
		errno = ENOENT;
		SPDLOG_DEBUG(
			"Performing stack trace lookup with key={}, not found, ENOENT",
			index);

		return nullptr;
	}
	SPDLOG_DEBUG(
		"Performing stack trace lookup with key={}, found, data size {}",
		index, itr->second.size());

	return itr->second.data();
}

long stack_trace_map_impl::elem_update(const void *key, const void *value,
				       uint64_t flags)
{
	SPDLOG_ERROR("You can't update a stack trace map");
	return -ENOTSUP;
}

long stack_trace_map_impl::elem_delete(const void *key)
{
	if (key == nullptr) {
		SPDLOG_ERROR(
			"You can't lookup a stack trace map with NULL key");
		errno = EINVAL;
		return -1;
	}
	uint32_t index = *(int *)key;
	if (auto itr = data.find(index); itr != data.end()) {
		data.erase(itr);
	} else {
		errno = ENOENT;
		return -1;
	}

	return 0;
}

// BPF_MAP_LOOKUP_AND_DELETE_ELEM on BPF_MAP_TYPE_STACK_TRACE. The kernel
// (bpf_stackmap_extract with delete=true) copies the recorded frames and
// zero-fills the rest of the value, so a shorter trace leaves the tail
// clear rather than exposing whatever the caller's buffer held.
int stack_trace_map_impl::lookup_and_delete(const void *key, void *value_out,
					    bool from_syscall)
{
	if (key == nullptr) {
		errno = EINVAL;
		return -1;
	}
	uint32_t index = *(uint32_t *)key;
	auto itr = data.find(index);
	if (itr == data.end()) {
		errno = ENOENT;
		return -1;
	}
	const auto value_size = max_stack_entries * sizeof(uint64_t);
	const auto trace_len =
		std::min(itr->second.size(), size_t(max_stack_entries)) *
		sizeof(uint64_t);
	std::memcpy(value_out, itr->second.data(), trace_len);
	std::memset((uint8_t *)value_out + trace_len, 0, value_size - trace_len);
	data.erase(itr);
	return 0;
}

int stack_trace_map_impl::map_get_next_key(const void *key, void *next_key)
{
	errno = ENOTSUP;
	SPDLOG_ERROR("get_next_key is not supported by stack_trace_map");
	return -1;
}

// Kernel __bpf_get_stackid semantics: frames beyond the value size are
// dropped, a stored trace is zero-padded to the full value (lookups copy
// value_size bytes), the same trace returns its id, and a different trace in
// an occupied bucket returns -EEXIST unless BPF_F_REUSE_STACKID replaces it.
// BPF_F_FAST_STACK_CMP compares the full hash instead of the frames.
int stack_trace_map_impl::fill_stack_trace(const std::vector<uint64_t> &stk,
					   bool discard_old_one,
					   bool compare_only_by_hash)
{
	const size_t nr = std::min<size_t>(stk.size(), max_stack_entries);
	const std::vector<uint64_t> frames(stk.begin(), stk.begin() + nr);
	const uint64_t full_hash = stack_trace_map::hash_stack_trace(frames);
	const uint32_t id = full_hash % this->max_entries;
	key_buf.assign(max_stack_entries, 0);
	std::copy(frames.begin(), frames.end(), key_buf.begin());
	auto itr = this->data.find(id);
	if (itr == this->data.end()) {
		data.emplace(stack_trace_map::value_ty(id, key_buf));
		return id;
	}
	bool equals;
	if (compare_only_by_hash) {
		// Stored traces never contain a zero frame; the padding does.
		auto stored_end = std::find(itr->second.begin(),
					    itr->second.end(), 0);
		std::vector<uint64_t> stored(itr->second.begin(), stored_end);
		equals = stack_trace_map::hash_stack_trace(stored) == full_hash;
	} else {
		equals = itr->second.size() == key_buf.size() &&
			 std::equal(key_buf.begin(), key_buf.end(),
				    itr->second.begin());
	}
	if (equals)
		return id;
	if (!discard_old_one)
		return -EEXIST;
	itr->second = key_buf;
	return id;
}

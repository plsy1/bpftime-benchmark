#include "bpf_map/map_common_def.hpp"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <stdexcept>

#include "stack_trace_map.hpp"
using namespace bpftime;

namespace
{
// Readers without the lock load the records while a lock holder writes them.
inline uint64_t load(const uint64_t *p, std::memory_order order)
{
	return std::atomic_ref<uint64_t>(*const_cast<uint64_t *>(p)).load(
		order);
}
inline void store(uint64_t *p, uint64_t value, std::memory_order order)
{
	std::atomic_ref<uint64_t>(*p).store(value, order);
}
} // namespace

stack_trace_map_impl::stack_trace_map_impl(
	boost::interprocess::managed_shared_memory &memory, uint32_t key_size,
	uint32_t value_size, uint32_t max_entries)
	: max_stack_entries(value_size / 8), max_entries(max_entries),
	  slots(max_entries, slot{ 0 },
		slot_allocator(memory.get_segment_manager())),
	  record_allocator(memory.get_segment_manager())
{
	if (key_size != 4) {
		SPDLOG_ERROR("Key size of stack trace map must be 4");
		throw std::runtime_error(
			"Key size of stack trace map must be 4");
	}
	if (value_size == 0 || value_size % 8 != 0) {
		SPDLOG_ERROR(
			"value_size of stack_trace_map must be a non-zero multiple of 8, unexpected: {}",
			value_size);
		throw std::runtime_error("Invalid value_size");
	}
	if (max_entries == 0) {
		SPDLOG_ERROR("max_entries of stack_trace_map must not be 0");
		throw std::runtime_error("Invalid max_entries");
	}
}

stack_trace_map_impl::~stack_trace_map_impl()
{
	for (const slot &s : slots)
		if (uint64_t *record = record_of(s))
			record_allocator.deallocate(
				boost::interprocess::offset_ptr<uint64_t>(
					record),
				REC_FRAMES + max_stack_entries);
}

uint64_t *stack_trace_map_impl::record_of(const slot &s) const
{
	// Acquire: the record is seen zeroed (ensure_record).
	const int64_t offset =
		std::atomic_ref<int64_t>(const_cast<int64_t &>(s.record))
			.load(std::memory_order_acquire);
	if (offset == 0)
		return nullptr;
	return (uint64_t *)((char *)const_cast<slot *>(&s) + offset);
}

uint64_t *stack_trace_map_impl::ensure_record(slot &s)
{
	if (uint64_t *record = record_of(s))
		return record;
	const size_t words = REC_FRAMES + max_stack_entries;
	uint64_t *record = record_allocator.allocate(words).get();
	std::fill(record, record + words, 0);
	std::atomic_ref<int64_t>(s.record).store((char *)record - (char *)&s,
						 std::memory_order_release);
	return record;
}

// The frame count goes to 0 first and back last (release): a reader that sees
// it non-zero sees this trace's frames and hash, or later ones.
void stack_trace_map_impl::store_trace(uint64_t *record, const uint64_t *ips,
				       size_t nr, uint64_t hash)
{
	store(&record[REC_NR], 0, std::memory_order_relaxed);
	std::atomic_thread_fence(std::memory_order_release);
	for (size_t i = 0; i < nr; i++)
		store(&record[REC_FRAMES + i], ips[i],
		      std::memory_order_relaxed);
	for (size_t i = nr; i < max_stack_entries; i++)
		store(&record[REC_FRAMES + i], 0, std::memory_order_relaxed);
	store(&record[REC_HASH], hash, std::memory_order_relaxed);
	store(&record[REC_NR], nr, std::memory_order_release);
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

	uint64_t *record =
		index < max_entries ? record_of(slots[index]) : nullptr;
	if (record == nullptr || record[REC_NR] == 0) {
		errno = ENOENT;
		SPDLOG_DEBUG(
			"Performing stack trace lookup with key={}, not found, ENOENT",
			index);

		return nullptr;
	}
	SPDLOG_DEBUG(
		"Performing stack trace lookup with key={}, found, {} frames",
		index, record[REC_NR]);

	return record + REC_FRAMES;
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
	uint64_t *record =
		index < max_entries ? record_of(slots[index]) : nullptr;
	if (record == nullptr || record[REC_NR] == 0) {
		errno = ENOENT;
		return -1;
	}
	// The frames stay until the next fill overwrites them.
	store(&record[REC_NR], 0, std::memory_order_relaxed);
	return 0;
}

int stack_trace_map_impl::map_get_next_key(const void *key, void *next_key)
{
	errno = ENOTSUP;
	SPDLOG_ERROR("get_next_key is not supported by stack_trace_map");
	return -1;
}

// As the kernel's __bpf_get_stackid without its lock: the hash is compared
// before the frames, so a mix of two traces read while a lock holder
// replaces one with the other cannot pass for a third trace (its frames may
// match, its hash was never stored). With BPF_F_FAST_STACK_CMP a reader may
// still match a trace being replaced, as in the kernel.
int stack_trace_map_impl::find_stack_trace(const uint64_t *ips, size_t nr,
					   bool compare_only_by_hash) const
{
	nr = std::min<size_t>(nr, max_stack_entries);
	if (nr == 0)
		return -1;
	const uint64_t hash = stack_trace_map::hash_stack_trace(ips, nr);
	const uint32_t id = hash % max_entries;
	const uint64_t *record = record_of(slots[id]);
	if (record == nullptr)
		return -1;
	const uint64_t stored_nr =
		load(&record[REC_NR], std::memory_order_acquire);
	if (stored_nr == 0 ||
	    load(&record[REC_HASH], std::memory_order_relaxed) != hash)
		return -1;
	if (compare_only_by_hash)
		return id;
	if (stored_nr != nr)
		return -1;
	for (size_t i = 0; i < nr; i++)
		if (load(&record[REC_FRAMES + i], std::memory_order_relaxed) !=
		    ips[i])
			return -1;
	return id;
}

// Kernel __bpf_get_stackid semantics: frames beyond the value size are
// dropped, a stored trace is zero-padded to the full value (lookups copy
// value_size bytes), the same trace returns its id, and a different trace in
// an occupied bucket returns -EEXIST unless BPF_F_REUSE_STACKID replaces it.
// BPF_F_FAST_STACK_CMP compares the full hash instead of the frames.
int stack_trace_map_impl::fill_stack_trace(const uint64_t *ips, size_t nr,
					   bool discard_old_one,
					   bool compare_only_by_hash)
{
	nr = std::min<size_t>(nr, max_stack_entries);
	if (nr == 0)
		return -EFAULT;
	const uint64_t hash = stack_trace_map::hash_stack_trace(ips, nr);
	const uint32_t id = hash % max_entries;
	slot &s = slots[id];
	// The lock holder is the only writer: plain reads.
	uint64_t *record = record_of(s);
	if (record != nullptr && record[REC_NR] != 0) {
		const bool equals =
			compare_only_by_hash ?
				record[REC_HASH] == hash :
				record[REC_NR] == nr &&
					std::equal(ips, ips + nr,
						   record + REC_FRAMES);
		if (equals)
			return id;
		if (!discard_old_one)
			return -EEXIST;
	} else if (record == nullptr) {
		// Called from a probe: no exception may leave.
		try {
			record = ensure_record(s);
		} catch (const std::exception &) {
			return -ENOMEM;
		}
	}
	store_trace(record, ips, nr, hash);
	return id;
}

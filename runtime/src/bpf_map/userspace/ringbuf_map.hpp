/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#ifndef _RINGBUF_MAP_HPP
#define _RINGBUF_MAP_HPP
#include <atomic>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/smart_ptr/deleter.hpp>
#include <boost/interprocess/smart_ptr/unique_ptr.hpp>
#include <boost/interprocess/smart_ptr/shared_ptr.hpp>
#include <boost/interprocess/smart_ptr/weak_ptr.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <cstddef>
#include <functional>
#include <optional>
#include <pthread.h>
#include <vector>

namespace bpftime
{
// The record header's second word (the kernel's pg_off; libbpf does not read
// it) is a tag: the map's fd, which bpf_ringbuf_submit and bpf_ringbuf_discard
// get from the record alone, and the reservation's owner slot plus one (0:
// none). Before, 5920d5f9 put the slot there in place of the fd, and the
// helpers looked the slot up as a map fd.
constexpr int ringbuf_tag_slot_bits = 9;
inline int32_t ringbuf_record_tag(int fd, int32_t slot)
{
	return (int32_t)(((uint32_t)fd << ringbuf_tag_slot_bits) |
			 (uint32_t)(slot + 1));
}
inline int ringbuf_tag_fd(int32_t tag)
{
	return (int)((uint32_t)tag >> ringbuf_tag_slot_bits);
}
inline int32_t ringbuf_tag_slot(int32_t tag)
{
	return (int32_t)((uint32_t)tag & ((1u << ringbuf_tag_slot_bits) - 1)) -
	       1;
}
// The fd of the map a reserved sample belongs to.
inline int ringbuf_record_fd(const void *sample)
{
	return ringbuf_tag_fd(((const int32_t *)sample)[-1]);
}

using sharable_mutex_ptr = boost::interprocess::managed_unique_ptr<
	boost::interprocess::interprocess_sharable_mutex,
	boost::interprocess::managed_shared_memory>::type;

using mutex_ptr = boost::interprocess::managed_unique_ptr<
	boost::interprocess::interprocess_mutex,
	boost::interprocess::managed_shared_memory>::type;

class ringbuf {
	using vec_allocator = boost::interprocess::allocator<
		char,
		boost::interprocess::managed_shared_memory::segment_manager>;
	using buf_vec = boost::interprocess::vector<char, vec_allocator>;
	using buf_vec_unique_ptr = boost::interprocess::managed_unique_ptr<
		buf_vec, boost::interprocess::managed_shared_memory>::type;
	uint32_t max_ent;
	uint32_t mask() const
	{
		return max_ent - 1;
	}
	boost::interprocess::offset_ptr<unsigned long> consumer_pos;
	boost::interprocess::offset_ptr<unsigned long> producer_pos;
	boost::interprocess::offset_ptr<uint8_t> data;
	// Guard for reserving memory
	// mutable mutex_ptr reserve_mutex;
	mutable pthread_spinlock_t reserve_spin_lock;
	// raw buffer
	buf_vec_unique_ptr raw_buffer;

	// Who holds each outstanding reservation, so that a consumer can skip
	// the record of a producer that died before submitting it (every later
	// record would wait behind it forever). The record header's tag holds
	// the slot, or -1 when the table was full (ringbuf_record_tag).
	struct reservation_owner {
		// The record's position + 1; 0 when the slot is free.
		std::atomic<uint64_t> position_plus_one{ 0 };
		int32_t pid = 0;
		int32_t tid = 0;
		uint64_t pid_namespace = 0;
	};
	static constexpr int reservation_owner_slots = 256;
	reservation_owner owners[reservation_owner_slots];
	int32_t claim_owner_slot(unsigned long position, unsigned long cons_pos,
				 int pid, int64_t tid, uint64_t pid_namespace);
	static bool discard_reserved(struct ringbuf_hdr *hdr, uint32_t len);

    public:
	bool has_data() const;
	// The tool's poll: if the next record is still reserved by a thread
	// of this pid namespace that no longer exists, mark it discarded.
	// True if it did.
	bool recover_dead_reservation();
	void *reserve(size_t size, int self_fd);
	// Publishes (or discards) a reserved record. False if a reclaim had
	// discarded it first.
	bool submit(const void *sample, bool discard);
	// Whether the consumer has caught up with this record (the next it
	// reads): bpf wakes a consumer then. Reads only the consumer position;
	// the record may be gone.
	bool consumer_at(const void *sample) const;
	int fetch_data(int (*)(void *ctx, void *data, size_t len), void *ctx);
	// Tests only: the record's owner slot, as in its header; overwrite
	// it; whether a slot is held; what recover_dead_reservation does to a
	// record; a submit that dies before releasing its slot.
	int32_t owner_slot_for_test(const void *sample) const;
	void set_owner_slot_for_test(const void *sample, int32_t slot);
	bool owner_slot_claimed_for_test(int32_t slot) const;
	bool discard_for_test(const void *sample);
	void publish_keeping_slot_for_test(const void *sample);
	ringbuf(uint32_t max_ent,
		boost::interprocess::managed_shared_memory &memory);
	friend class ringbuf_map_impl;
	~ringbuf();
};

using ringbuf_shared_ptr = boost::interprocess::managed_shared_ptr<
	ringbuf,
	boost::interprocess::managed_shared_memory::segment_manager>::type;
using ringbuf_weak_ptr = boost::interprocess::managed_weak_ptr<
	ringbuf,
	boost::interprocess::managed_shared_memory::segment_manager>::type;

// implementation of ringbuf map
class ringbuf_map_impl {
	ringbuf_shared_ptr ringbuf_impl;

    public:
	const static bool should_lock = false;
	ringbuf_map_impl(uint32_t max_ent,
			 boost::interprocess::managed_shared_memory &memory);

	void *elem_lookup(const void *key);

	long elem_update(const void *key, const void *value, uint64_t flags);

	long elem_delete(const void *key);

	int map_get_next_key(const void *key, void *next_key);
	ringbuf_weak_ptr create_impl_weak_ptr();
	ringbuf_shared_ptr create_impl_shared_ptr();

	void *get_consumer_page() const;
	void *get_producer_page() const;
	void *reserve(size_t size, int self_fd);
	bool submit(const void *sample, bool discard);
	bool consumer_at(const void *sample) const;
};

} // namespace bpftime
#endif

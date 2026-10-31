/* SPDX-License-Identifier: MIT */
#include <catch2/catch_test_macros.hpp>

#if __linux__
#include "bpftime_shm.hpp"
#include "common_def.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <boost/scope_exit.hpp>
#include <linux/bpf.h>
#include <string>
#include <unistd.h>
#include <vector>
#include <cstring>
#include <sys/wait.h>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <bpf_map/userspace/ringbuf_map.hpp>

// Ringbuf records are 8-byte headers followed by the payload, both kept
// adjacent in a power-of-two data area. When a header lands in the very
// last 8 bytes of the ring, the payload no longer fits behind it inside
// the ring; the record must be kept contiguous instead of wrapping the
// payload back to the start of the data area.
TEST_CASE("ringbuf keeps tail-straddling records contiguous and consumable",
	  "[ringbuf]")
{
	const char *old_name = getenv("BPFTIME_GLOBAL_SHM_NAME");
	const bool had_name = old_name != nullptr;
	const std::string previous_name = old_name ? old_name : "";
	const std::string name = "RingbufTailShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(name) };
	bool initialized = false;
	BOOST_SCOPE_EXIT_ALL(&)
	{
		if (initialized) {
			bpftime_destroy_global_shm();
			bpftime_initialize_global_shm(
				bpftime::shm_open_type::SHM_NO_CREATE);
		}
		if (had_name)
			setenv("BPFTIME_GLOBAL_SHM_NAME", previous_name.c_str(),
			       1);
		else
			unsetenv("BPFTIME_GLOBAL_SHM_NAME");
	};
	REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
	bpftime_destroy_global_shm();
	bpftime_initialize_global_shm(
		bpftime::shm_open_type::SHM_REMOVE_AND_CREATE);
	initialized = true;

	struct rec {
		uint64_t seq;
		uint8_t pad[40];
	};
	static_assert(sizeof(rec) == 48, "reproducer record size");

	// 48-byte records occupy 56 bytes on the ring (48 payload + 8
	// header), and 56 * 73 = 4088 = 4096 - 8, so record 74 puts its
	// header in the last 8 bytes of the ring.
	const int rb_fd = bpftime_maps_create(
		-1, "tail_wrap_ring",
		{ .type = BPF_MAP_TYPE_RINGBUF, .max_ents = 4096 });
	REQUIRE(rb_fd >= 0);

	uint64_t received = 0;
	auto drain = +[](void *ctx, void *data, size_t len) -> int {
		REQUIRE(len == sizeof(rec));
		auto *out = static_cast<uint64_t *>(ctx);
		const auto *r = static_cast<const rec *>(data);
		REQUIRE(r->seq == *out);
		(*out)++;
		return 0;
	};

	// Reserve + submit one record at a time and drain it, so every
	// record is written exactly at the current producer offset and the
	// wrap position is exercised deterministically.
	for (uint64_t seq = 0; seq < 200; ++seq) {
		auto *payload = bpftime_ringbuf_reserve(rb_fd, sizeof(rec));
		REQUIRE(payload != nullptr);
		auto *r = static_cast<rec *>(payload);
		r->seq = seq;
		for (auto &b : r->pad)
			b = 0;
		bpftime_ringbuf_submit(rb_fd, payload, 0);

		const int drained =
			bpftime_poll_from_ringbuf(rb_fd, &received, drain);
		REQUIRE(drained == 1);
		REQUIRE(received == seq + 1);
	}
}

#endif /* __linux__ */

TEST_CASE("Ringbuf skips the record of a producer that died before submitting",
	  "[ringbuf]")
{
	// The kernel cannot have a reservation outlive its program; here a
	// traced process killed between bpf_ringbuf_reserve and submit left a
	// busy header that held back every later record, silently.
	const std::string name =
		"bpftime_ringbuf_dead_owner_" + std::to_string(getpid());
	boost::interprocess::shared_memory_object::remove(name.c_str());
	{
		boost::interprocess::managed_shared_memory shm(
			boost::interprocess::create_only, name.c_str(),
			4 << 20);
		auto *map = shm.construct<bpftime::ringbuf_map_impl>(
			"ringbuf")(4096, shm);
		auto rb = map->create_impl_shared_ptr();
		auto count = [&] {
			return rb->fetch_data(
				[](void *, void *, size_t) { return 0; },
				nullptr);
		};
		// A live producer's reservation is not taken away.
		void *mine = map->reserve(16, 1);
		REQUIRE(mine != nullptr);
		CHECK_FALSE(rb->recover_dead_reservation());
		map->submit(mine, false);
		CHECK(count() == 1);

		pid_t child = fork();
		REQUIRE(child >= 0);
		if (child == 0)
			_exit(map->reserve(16, 1) != nullptr ? 0 : 1);
		int status = -1;
		REQUIRE(waitpid(child, &status, 0) == child);
		REQUIRE(WEXITSTATUS(status) == 0);
		for (int i = 0; i < 10; i++) {
			void *p = map->reserve(16, 1);
			REQUIRE(p != nullptr);
			std::memset(p, 0x5a, 16);
			map->submit(p, false);
		}
		CHECK(count() == 0);
		CHECK_FALSE(rb->has_data());
		CHECK(rb->recover_dead_reservation());
		CHECK(rb->has_data());
		CHECK(count() == 10);
	}
	boost::interprocess::shared_memory_object::remove(name.c_str());
}

// submit used to read the record's owner slot after publishing it: a lap
// later the bytes may be another record's header (or padding, which holds
// the map fd), and the slot it named was freed under a live reservation.
// A record whose header no longer names this thread's slot for it is no
// longer this thread's: submit leaves it alone.
TEST_CASE("Ringbuf submit releases only its own owner slot", "[ringbuf]")
{
	const std::string name =
		"bpftime_ringbuf_own_slot_" + std::to_string(getpid());
	boost::interprocess::shared_memory_object::remove(name.c_str());
	{
		boost::interprocess::managed_shared_memory shm(
			boost::interprocess::create_only, name.c_str(),
			4 << 20);
		auto *map = shm.construct<bpftime::ringbuf_map_impl>(
			"ringbuf")(4096, shm);
		auto rb = map->create_impl_shared_ptr();
		void *a = map->reserve(16, 1);
		void *b = map->reserve(16, 1);
		REQUIRE(a != nullptr);
		REQUIRE(b != nullptr);
		const int32_t slot_b = rb->owner_slot_for_test(b);
		REQUIRE(slot_b >= 0);
		// a's header bytes now name b's slot, as after a lap.
		rb->set_owner_slot_for_test(a, slot_b);
		map->submit(a, false);
		CHECK(rb->owner_slot_claimed_for_test(slot_b));
		map->submit(b, false);
		CHECK_FALSE(rb->owner_slot_claimed_for_test(slot_b));
	}
	boost::interprocess::shared_memory_object::remove(name.c_str());
}

// A record recover_dead_reservation discarded (its owner looked dead: a
// stale identity after a raw clone) is not published again by a late
// submit, and the slot, given to another reservation since, stays held.
TEST_CASE("Ringbuf submit leaves a recovered record alone", "[ringbuf]")
{
	const std::string name =
		"bpftime_ringbuf_late_submit_" + std::to_string(getpid());
	boost::interprocess::shared_memory_object::remove(name.c_str());
	{
		boost::interprocess::managed_shared_memory shm(
			boost::interprocess::create_only, name.c_str(),
			4 << 20);
		auto *map = shm.construct<bpftime::ringbuf_map_impl>(
			"ringbuf")(4096, shm);
		auto rb = map->create_impl_shared_ptr();
		void *a = map->reserve(16, 1);
		REQUIRE(a != nullptr);
		const int32_t slot_a = rb->owner_slot_for_test(a);
		REQUIRE(rb->discard_for_test(a));
		void *b = map->reserve(16, 1);
		REQUIRE(b != nullptr);
		// The same thread starts at the same slot.
		REQUIRE(rb->owner_slot_for_test(b) == slot_a);
		map->submit(a, false);
		CHECK(rb->owner_slot_claimed_for_test(slot_a));
		int delivered = rb->fetch_data(
			[](void *, void *, size_t) { return 0; }, nullptr);
		// a stays discarded; b is not submitted yet.
		CHECK(delivered == 0);
		map->submit(b, false);
		delivered = rb->fetch_data(
			[](void *, void *, size_t) { return 0; }, nullptr);
		CHECK(delivered == 1);
	}
	boost::interprocess::shared_memory_object::remove(name.c_str());
}

// A producer killed between publishing and releasing its slot leaked the
// slot; after 256 such every reservation had none (-1) and could not be
// recovered. A consumed record's slot is free.
TEST_CASE("Ringbuf owner slots of consumed records are reused", "[ringbuf]")
{
	const std::string name =
		"bpftime_ringbuf_slot_reuse_" + std::to_string(getpid());
	boost::interprocess::shared_memory_object::remove(name.c_str());
	{
		boost::interprocess::managed_shared_memory shm(
			boost::interprocess::create_only, name.c_str(),
			4 << 20);
		auto *map = shm.construct<bpftime::ringbuf_map_impl>(
			"ringbuf")(4096, shm);
		auto rb = map->create_impl_shared_ptr();
		for (int i = 0; i < 300; i++) {
			void *p = map->reserve(16, 1);
			REQUIRE(p != nullptr);
			INFO("reservation " << i);
			REQUIRE(rb->owner_slot_for_test(p) >= 0);
			rb->publish_keeping_slot_for_test(p);
			REQUIRE(rb->fetch_data(
					[](void *, void *, size_t) { return 0; },
					nullptr) == 1);
		}
	}
	boost::interprocess::shared_memory_object::remove(name.c_str());
}

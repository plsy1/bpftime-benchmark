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

// tgkill finds a thread-group leader until its parent reaps it: the record
// of a single-threaded producer that died stayed in the way until then (for
// ever if the parent never waits).
TEST_CASE("Ringbuf recovers a dead producer before it is reaped", "[ringbuf]")
{
	const std::string name =
		"bpftime_ringbuf_zombie_" + std::to_string(getpid());
	boost::interprocess::shared_memory_object::remove(name.c_str());
	{
		boost::interprocess::managed_shared_memory shm(
			boost::interprocess::create_only, name.c_str(),
			4 << 20);
		auto *map = shm.construct<bpftime::ringbuf_map_impl>(
			"ringbuf")(4096, shm);
		auto rb = map->create_impl_shared_ptr();
		pid_t child = fork();
		REQUIRE(child >= 0);
		if (child == 0)
			_exit(map->reserve(16, 1) != nullptr ? 0 : 1);
		// Until the child is a zombie (not reaped).
		char state = 0;
		for (int i = 0; i < 5000 && state != 'Z'; i++) {
			const std::string path =
				"/proc/" + std::to_string(child) + "/stat";
			FILE *f = fopen(path.c_str(), "r");
			if (f != nullptr) {
				char buf[512] = {};
				fread(buf, 1, sizeof(buf) - 1, f);
				fclose(f);
				const char *end = strrchr(buf, ')');
				if (end != nullptr)
					state = end[2];
			}
			if (state != 'Z')
				usleep(1000);
		}
		REQUIRE(state == 'Z');
		CHECK(rb->recover_dead_reservation());
		int status = -1;
		REQUIRE(waitpid(child, &status, 0) == child);
		CHECK(WEXITSTATUS(status) == 0);
	}
	boost::interprocess::shared_memory_object::remove(name.c_str());
}

extern "C" uint64_t bpf_ringbuf_reserve(uint64_t rb, uint64_t size,
					uint64_t flags, uint64_t, uint64_t);
extern "C" uint64_t bpf_ringbuf_submit(uint64_t data, uint64_t flags, uint64_t,
				       uint64_t, uint64_t);
extern "C" uint64_t bpf_ringbuf_discard(uint64_t data, uint64_t flags,
					uint64_t, uint64_t, uint64_t);

// bpf_ringbuf_submit and bpf_ringbuf_discard find the map from the sample
// alone, through the record header's tag. 5920d5f9 put the owner slot there
// in place of the fd: the helpers looked the slot up as a map fd ("Expected
// fd 145 to be an ringbuf map fd") and the record stayed reserved.
TEST_CASE("Ringbuf helpers submit and discard through the sample's map",
	  "[ringbuf]")
{
	const char *old_name = getenv("BPFTIME_GLOBAL_SHM_NAME");
	const bool had_name = old_name != nullptr;
	const std::string previous_name = old_name ? old_name : "";
	const std::string name = "RingbufHelperShm-" + std::to_string(getpid());
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

	// Another map first, so neither ringbuf has fd 0 or a slot's number.
	REQUIRE(bpftime_maps_create(-1, "not_a_ringbuf",
				    { .type = BPF_MAP_TYPE_ARRAY,
				      .key_size = 4,
				      .value_size = 8,
				      .max_ents = 1 }) >= 0);
	for (const char *map_name : { "ringbuf_a", "ringbuf_b" }) {
		const int fd = bpftime_maps_create(
			-1, map_name,
			{ .type = BPF_MAP_TYPE_RINGBUF, .max_ents = 4096 });
		REQUIRE(fd >= 0);
		INFO("map " << map_name << ", fd " << fd);
		auto header_len = [](void *sample) {
			return __atomic_load_n((uint32_t *)sample - 2,
					       __ATOMIC_ACQUIRE);
		};
		auto *kept = (void *)(uintptr_t)bpf_ringbuf_reserve(fd, 16, 0,
								     0, 0);
		REQUIRE(kept != nullptr);
		CHECK(bpftime_ringbuf_record_fd(kept) == fd);
		auto *dropped = (void *)(uintptr_t)bpf_ringbuf_reserve(
			fd, 16, 0, 0, 0);
		REQUIRE(dropped != nullptr);
		CHECK(bpftime_ringbuf_record_fd(dropped) == fd);
		bpf_ringbuf_submit((uint64_t)(uintptr_t)kept, 0, 0, 0, 0);
		bpf_ringbuf_discard((uint64_t)(uintptr_t)dropped, 0, 0, 0, 0);
		CHECK((header_len(kept) &
		       (BPF_RINGBUF_BUSY_BIT | BPF_RINGBUF_DISCARD_BIT)) == 0);
		CHECK((header_len(dropped) & BPF_RINGBUF_BUSY_BIT) == 0);
		CHECK((header_len(dropped) & BPF_RINGBUF_DISCARD_BIT) != 0);
	}
}

#if __linux__
#include <chrono>
#include <sched.h>

namespace
{
constexpr int ringbuf_mp_procs = 4;
constexpr uint32_t ringbuf_mp_records = 100000;

// Sizes 8..120 bytes, varying so that records pad at the ring's end.
uint32_t ringbuf_mp_size(uint32_t proc, uint32_t seq)
{
	return 8 + ((seq * 7 + proc * 13) % 15) * 8;
}

struct ringbuf_mp_seen {
	std::vector<std::vector<uint8_t>> count;
	uint64_t got = 0;
	uint64_t bad = 0;
};

int ringbuf_mp_check(void *ctx, void *data, size_t len)
{
	auto *seen = (ringbuf_mp_seen *)ctx;
	auto *words = (const uint32_t *)data;
	if (len < 8 || words[0] >= (uint32_t)ringbuf_mp_procs ||
	    words[1] >= ringbuf_mp_records ||
	    len != ringbuf_mp_size(words[0], words[1])) {
		seen->bad++;
		return 0;
	}
	for (uint32_t w = 2; w < len / 4; w++) {
		if (words[w] != words[0] * 31 + words[1] + w) {
			seen->bad++;
			return 0;
		}
	}
	seen->count[words[0]][words[1]]++;
	seen->got++;
	return 0;
}
} // namespace

// Producers in several processes reserve and submit at once while the
// consumer drains: every record arrives exactly once, whole (the reserve
// lock and what it covers; uprobe/improve-20261106 item 4).
TEST_CASE("Ringbuf producers in several processes deliver every record once",
	  "[ringbuf][ringbuf-mp]")
{
	const std::string name =
		"bpftime_ringbuf_mp_" + std::to_string(getpid());
	boost::interprocess::shared_memory_object::remove(name.c_str());
	{
		boost::interprocess::managed_shared_memory shm(
			boost::interprocess::create_only, name.c_str(),
			8 << 20);
		auto *map = shm.construct<bpftime::ringbuf_map_impl>(
			"ringbuf")(1 << 16, shm);
		auto rb = map->create_impl_shared_ptr();
		std::vector<pid_t> children;
		for (int proc = 0; proc < ringbuf_mp_procs; proc++) {
			pid_t child = fork();
			REQUIRE(child >= 0);
			if (child == 0) {
				for (uint32_t seq = 0; seq < ringbuf_mp_records;) {
					const uint32_t size =
						ringbuf_mp_size(proc, seq);
					auto *words = (uint32_t *)map->reserve(
						size, 1);
					if (words == nullptr) {
						sched_yield(); // full
						continue;
					}
					words[0] = proc;
					words[1] = seq;
					for (uint32_t w = 2; w < size / 4; w++)
						words[w] = proc * 31 + seq + w;
					map->submit(words, false);
					seq++;
				}
				_exit(0);
			}
			children.push_back(child);
		}
		ringbuf_mp_seen seen;
		seen.count.assign(ringbuf_mp_procs,
				  std::vector<uint8_t>(ringbuf_mp_records, 0));
		const uint64_t total =
			(uint64_t)ringbuf_mp_procs * ringbuf_mp_records;
		const auto deadline =
			std::chrono::steady_clock::now() + std::chrono::seconds(120);
		while (seen.got + seen.bad < total &&
		       std::chrono::steady_clock::now() < deadline) {
			if (rb->fetch_data(ringbuf_mp_check, &seen) == 0)
				sched_yield();
		}
		for (pid_t child : children) {
			int status = -1;
			REQUIRE(waitpid(child, &status, 0) == child);
			CHECK(WIFEXITED(status));
			CHECK(WEXITSTATUS(status) == 0);
		}
		rb->fetch_data(ringbuf_mp_check, &seen);
		uint64_t missing = 0, repeated = 0;
		for (const auto &per_proc : seen.count)
			for (uint8_t n : per_proc) {
				missing += n == 0;
				repeated += n > 1;
			}
		CHECK(seen.bad == 0);
		CHECK(missing == 0);
		CHECK(repeated == 0);
		CHECK(seen.got == total);
	}
	boost::interprocess::shared_memory_object::remove(name.c_str());
}
#endif

#if __linux__
// Every reserve writes the lock twice; the fields before it are read by every
// submit and by the consumer (uprobe/improve-20261106 item 4).
TEST_CASE("Ringbuf reserve lock has a cache line of its own", "[ringbuf]")
{
	const std::string name =
		"bpftime_ringbuf_lock_line_" + std::to_string(getpid());
	boost::interprocess::shared_memory_object::remove(name.c_str());
	{
		boost::interprocess::managed_shared_memory shm(
			boost::interprocess::create_only, name.c_str(),
			1 << 20);
		// Several objects, so that their addresses fall at
		// different offsets within a cache line.
		std::vector<bpftime::ringbuf_map_impl *> maps;
		for (int i = 0; i < 16; i++) {
			maps.push_back(shm.construct<bpftime::ringbuf_map_impl>(
				boost::interprocess::anonymous_instance)(4096,
									 shm));
			auto padding = shm.allocate(8 * (i + 1));
			(void)padding;
		}
		int alone = 0;
		for (auto *map : maps)
			alone += map->create_impl_shared_ptr()
					 ->reserve_lock_line_alone_for_test();
		CHECK(alone == (int)maps.size());
	}
	boost::interprocess::shared_memory_object::remove(name.c_str());
}
#endif

#if __linux__
// Reserve keeps the consumer position it read last and reads consumer_pos
// again only when that leaves too little space (uprobe/improve-20261106
// item 4): space the consumer freed since is found then.
TEST_CASE("Ringbuf reserve finds space the consumer freed", "[ringbuf]")
{
	const std::string name =
		"bpftime_ringbuf_cached_cons_" + std::to_string(getpid());
	boost::interprocess::shared_memory_object::remove(name.c_str());
	{
		boost::interprocess::managed_shared_memory shm(
			boost::interprocess::create_only, name.c_str(),
			1 << 20);
		auto *map = shm.construct<bpftime::ringbuf_map_impl>(
			"ringbuf")(4096, shm);
		auto rb = map->create_impl_shared_ptr();
		auto drain = [&] {
			return rb->fetch_data(
				[](void *, void *, size_t) { return 0; },
				nullptr);
		};
		for (int lap = 0; lap < 3; lap++) {
			int reserved = 0;
			while (void *p = map->reserve(48, 1)) {
				map->submit(p, false);
				reserved++;
			}
			CHECK(errno == ENOSPC);
			CHECK(reserved >= 72); // 56 bytes each in 4096
			CHECK(drain() == reserved);
		}
	}
	boost::interprocess::shared_memory_object::remove(name.c_str());
}
#endif

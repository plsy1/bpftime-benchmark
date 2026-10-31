#include "catch2/catch_test_macros.hpp"

#include "handler/perf_event_handler.hpp"
#include "bpftime_shm.hpp"
#include "common_def.hpp"
#include <atomic>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <limits>
#include <string>
#include <thread>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <utility>
#include <unistd.h>
#include <vector>

#if __linux__
#include <linux/bpf.h>
#include <sched.h>

extern "C" uint64_t bpf_perf_event_output(uint64_t, uint64_t, uint64_t,
					  uint64_t, uint64_t);

TEST_CASE("Perf output preserves the calling thread's CPU affinity",
	  "[perf_event][affinity]")
{
	cpu_set_t before;
	REQUIRE(sched_getaffinity(0, sizeof(before), &before) == 0);
	if (CPU_COUNT(&before) < 2)
		SKIP("Affinity restoration requires at least two allowed CPUs");

	const char *old_name = getenv("BPFTIME_GLOBAL_SHM_NAME");
	struct cleanup {
		cpu_set_t affinity;
		bool had_name;
		std::string old_name;
		int map_fd = -1;
		int perf_fd = -1;
		~cleanup()
		{
			sched_setaffinity(0, sizeof(affinity), &affinity);
			for (int fd : { map_fd, perf_fd }) {
				if (fd >= 0) {
					bpftime_close(fd);
					close(fd);
				}
			}
			bpftime_destroy_global_shm();
			bpftime_initialize_global_shm(
				bpftime::shm_open_type::SHM_NO_CREATE);
			if (had_name)
				setenv("BPFTIME_GLOBAL_SHM_NAME",
				       old_name.c_str(), 1);
			else
				unsetenv("BPFTIME_GLOBAL_SHM_NAME");
		}
	} cleanup{ before, old_name != nullptr, old_name ? old_name : "" };
	const std::string name =
		"PerfOutputAffinityTestShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(name) };
	REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
	bpftime_destroy_global_shm();
	bpftime_initialize_global_shm(
		bpftime::shm_open_type::SHM_REMOVE_AND_CREATE);

	int &map_fd = cleanup.map_fd;
	uint64_t expected_ret = static_cast<uint64_t>(-1);
	int expected_errno = ENOENT;
	SECTION("Unknown map")
	{
	}
	SECTION("CPU index outside the perf array")
	{
		// A one-entry map has no slot for any CPU other than CPU 0.
		CPU_CLR(0, &before);
		if (CPU_COUNT(&before) < 2)
			SKIP("This case requires two allowed nonzero CPUs");
		REQUIRE(sched_setaffinity(0, sizeof(before), &before) == 0);
		map_fd = bpftime_maps_create(
			-1, "short_perf_array",
			{ .type = BPF_MAP_TYPE_PERF_EVENT_ARRAY,
			  .key_size = 4,
			  .value_size = 4,
			  .max_ents = 1 });
		REQUIRE(map_fd >= 0);
		expected_errno = EINVAL;
	}
	SECTION("Successful output")
	{
		map_fd = bpftime_maps_create(
			-1, "perf_array",
			{ .type = BPF_MAP_TYPE_PERF_EVENT_ARRAY,
			  .key_size = 4,
			  .value_size = 4,
			  .max_ents = CPU_SETSIZE });
		REQUIRE(map_fd >= 0);
		int &perf_fd = cleanup.perf_fd;
		perf_fd = bpftime_add_software_perf_event(0, 0, 0);
		REQUIRE(perf_fd >= 0);
		REQUIRE(bpftime_get_software_perf_event_raw_buffer(
				perf_fd, 2 * getpagesize()) != nullptr);
		for (int cpu = 0; cpu < CPU_SETSIZE; cpu++) {
			if (CPU_ISSET(cpu, &before))
				REQUIRE(bpftime_map_update_elem(map_fd, &cpu,
								&perf_fd,
								0) == 0);
		}
		expected_ret = 0;
		expected_errno = 0;
	}

	uint32_t payload = 42;
	errno = 0;
	const auto ret =
		bpf_perf_event_output(0, map_fd, 0,
				      reinterpret_cast<uint64_t>(&payload),
				      sizeof(payload));
	const int output_errno = errno;
	cpu_set_t after;
	REQUIRE(sched_getaffinity(0, sizeof(after), &after) == 0);
	CHECK(ret == expected_ret);
	if (expected_ret == static_cast<uint64_t>(-1))
		CHECK(output_errno == expected_errno);
	CHECK(CPU_EQUAL(&before, &after));
}
#endif

namespace
{
struct event_payload {
	int producer;
	int sequence;
};

constexpr size_t aligned_perf_record_size(size_t payload_size)
{
	constexpr size_t alignment = sizeof(uint64_t);
	return (sizeof(bpftime::perf_sample_raw) + payload_size + alignment -
		1) &
	       ~(alignment - 1);
}

void copy_from_perf_ring(uint8_t *base, size_t ring_size, uint64_t offset,
			 void *dst, size_t size)
{
	uint8_t *copy_start_1 = base + (offset & (ring_size - 1));
	if (size + copy_start_1 <= base + ring_size) {
		memcpy(dst, copy_start_1, size);
	} else {
		size_t len_first = base + ring_size - copy_start_1;
		size_t len_second = size - len_first;
		memcpy(dst, copy_start_1, len_first);
		memcpy((uint8_t *)dst + len_first, base, len_second);
	}
}
} // namespace

TEST_CASE("Software perf event buffers shard concurrent producers by thread",
	  "[perf_event][software_perf_event]")
{
	const std::string shared_memory_name =
		"SoftwarePerfEventShardTestShm-" + std::to_string(getpid());
	const size_t shared_memory_size = 16 * 1024 * 1024;
	shm_remove remover{ std::string(shared_memory_name) };

	boost::interprocess::managed_shared_memory shm(
		boost::interprocess::create_only, shared_memory_name.c_str(),
		shared_memory_size);

	auto *perf = shm.construct<bpftime::software_perf_event_data>(
		"perf")(0, 0, 0, shm);
	REQUIRE(perf != nullptr);

	const size_t ring_size = 1024 * 1024;
	void *raw_buffer = perf->ensure_mmap_buffer(getpagesize() + ring_size);
	REQUIRE(raw_buffer != nullptr);

	constexpr int producer_count = 4;
	constexpr int events_per_producer = 256;
	std::atomic<bool> start{ false };
	std::atomic<bool> output_failed{ false };
	std::vector<std::thread> producers;
	producers.reserve(producer_count);
	for (int producer = 0; producer < producer_count; producer++) {
		producers.emplace_back([&, producer]() {
			while (!start.load(std::memory_order_acquire)) {
				std::this_thread::yield();
			}
			for (int sequence = 0; sequence < events_per_producer;
			     sequence++) {
				event_payload payload{ producer, sequence };
				if (perf->output_data(&payload,
						      sizeof(payload)) != 0) {
					output_failed.store(
						true,
						std::memory_order_release);
				}
			}
		});
	}

	start.store(true, std::memory_order_release);
	for (auto &producer : producers) {
		producer.join();
	}
	REQUIRE_FALSE(output_failed.load(std::memory_order_acquire));

	REQUIRE(perf->has_data());
	for (int i = 0; i < 64; i++) {
		REQUIRE(perf->has_data());
	}
	REQUIRE(perf->producer_shards.empty());

	auto *header = (perf_event_mmap_page *)raw_buffer;
	auto *base = (uint8_t *)raw_buffer + getpagesize();
	uint64_t tail = header->data_tail;
	uint64_t head = header->data_head;
	REQUIRE(head > tail);

	std::vector<int> seen(producer_count * events_per_producer, 0);
	int record_count = 0;
	while (tail < head) {
		bpftime::perf_sample_raw sample;
		copy_from_perf_ring(base, ring_size, tail, &sample,
				    sizeof(sample));
		REQUIRE(sample.header.type == PERF_RECORD_SAMPLE);
		REQUIRE(sample.header.size ==
			aligned_perf_record_size(sizeof(event_payload)));
		REQUIRE(sample.size ==
			sample.header.size - sizeof(bpftime::perf_sample_raw));

		event_payload payload;
		copy_from_perf_ring(base, ring_size,
				    tail + sizeof(bpftime::perf_sample_raw),
				    &payload, sizeof(payload));
		REQUIRE(payload.producer >= 0);
		REQUIRE(payload.producer < producer_count);
		REQUIRE(payload.sequence >= 0);
		REQUIRE(payload.sequence < events_per_producer);
		seen[payload.producer * events_per_producer +
		     payload.sequence]++;
		record_count++;
		tail += sample.header.size;
	}

	REQUIRE(record_count == producer_count * events_per_producer);
	for (int count : seen) {
		REQUIRE(count == 1);
	}

	constexpr size_t max_payload_size =
		std::numeric_limits<uint16_t>::max() -
		sizeof(bpftime::perf_sample_raw) - (sizeof(uint64_t) - 1);
	std::vector<uint8_t> boundary_payload(max_payload_size + 1);
	const uint64_t head_before_boundary = header->data_head;
	REQUIRE(perf->output_data(boundary_payload.data(), max_payload_size) ==
		0);
	REQUIRE(perf->has_data());
	REQUIRE(header->data_head - head_before_boundary ==
		aligned_perf_record_size(max_payload_size));
	errno = 0;
	REQUIRE(perf->output_data(boundary_payload.data(),
				  boundary_payload.size()) == -1);
	REQUIRE(errno == E2BIG);
}

TEST_CASE("Software perf event fork child writes to its own producer shard",
	  "[perf_event][software_perf_event]")
{
	const std::string shared_memory_name =
		"SoftwarePerfEventForkShardTestShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(shared_memory_name) };
	boost::interprocess::managed_shared_memory shm(
		boost::interprocess::create_only, shared_memory_name.c_str(),
		16 * 1024 * 1024);
	auto *perf = shm.construct<bpftime::software_perf_event_data>(
		"perf")(0, 0, 0, shm);
	REQUIRE(perf != nullptr);
	REQUIRE(perf->ensure_mmap_buffer(getpagesize() + 1024 * 1024) !=
		nullptr);

	// The parent's first sample caches its (pid, tid) for this thread.
	event_payload parent_payload{ 1, 0 };
	REQUIRE(perf->output_data(&parent_payload, sizeof(parent_payload)) ==
		0);
	const pid_t parent = getpid();
	pid_t child = fork();
	REQUIRE(child >= 0);
	if (child == 0) {
		event_payload child_payload{ 2, 0 };
		bool ok = perf->output_data(&child_payload,
					    sizeof(child_payload)) == 0;
		const auto self_tid = (int64_t)syscall(SYS_gettid);
		// With the parent's cached identity the sample would have
		// gone to the parent's shard and no child shard would exist.
		bool own_shard = false;
		for (auto &shard : perf->producer_shards)
			own_shard |= shard.pid == getpid() &&
				     shard.tid == self_tid;
		_exit(ok && own_shard ? 0 : 1);
	}
	int status = -1;
	REQUIRE(waitpid(child, &status, 0) == child);
	REQUIRE(WIFEXITED(status));
	REQUIRE(WEXITSTATUS(status) == 0);
	size_t parent_shards = 0, child_shards = 0;
	for (auto &shard : perf->producer_shards) {
		parent_shards += shard.pid == parent;
		child_shards += shard.pid == child;
	}
	CHECK(parent_shards == 1);
	CHECK(child_shards == 1);
	// The parent keeps using its own cached identity and shard.
	REQUIRE(perf->output_data(&parent_payload, sizeof(parent_payload)) ==
		0);
	parent_shards = 0;
	for (auto &shard : perf->producer_shards)
		parent_shards += shard.pid == parent;
	CHECK(parent_shards == 1);
}

// Idle shards of dead threads are reclaimed by probing (pid, tid) with
// tgkill. A producer in another pid namespace (a container sharing /dev/shm
// with the tool) has pids that mean nothing here: its live, idle shard was
// reclaimed, discarding its lost-sample count.
TEST_CASE("Software perf event shards of another pid namespace are not reclaimed",
	  "[perf_event][software_perf_event]")
{
	const std::string shared_memory_name =
		"SoftwarePerfEventPidNsTestShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(shared_memory_name) };
	boost::interprocess::managed_shared_memory shm(
		boost::interprocess::create_only, shared_memory_name.c_str(),
		16 * 1024 * 1024);
	auto *perf = shm.construct<bpftime::software_perf_event_data>(
		"perf")(0, 0, 0, shm);
	REQUIRE(perf != nullptr);
	REQUIRE(perf->ensure_mmap_buffer(getpagesize() + 64 * 1024) !=
		nullptr);
	// The parent's own shard, so the namespace recorded is this one.
	event_payload payload{ 1, 0 };
	REQUIRE(perf->output_data(&payload, sizeof(payload)) == 0);
	const uint64_t here = perf->producer_shards.front().pid_namespace;
	REQUIRE(here != 0);
	// A pid with no thread: a reaped child's.
	pid_t dead = fork();
	REQUIRE(dead >= 0);
	if (dead == 0)
		_exit(0);
	REQUIRE(waitpid(dead, nullptr, 0) == dead);
	auto *manager = shm.get_segment_manager();
	for (uint64_t ns : { here, here + 1 }) {
		perf->producer_shards.emplace_back(dead, dead, 1000 + ns, 0,
						   getpagesize(), manager,
						   getpagesize() + 4096);
		perf->producer_shards.back().pid_namespace = ns;
	}
	// Draining reclaims idle shards every 64 drains.
	for (int i = 0; i < 130; i++)
		(void)perf->has_data();
	size_t same_namespace = 0, other_namespace = 0;
	for (auto &shard : perf->producer_shards) {
		if (shard.pid != dead)
			continue;
		same_namespace += shard.pid_namespace == here;
		other_namespace += shard.pid_namespace == here + 1;
	}
	CHECK(same_namespace == 0);
	CHECK(other_namespace == 1);
}

TEST_CASE("Software perf event records wrap on aligned boundaries after resize",
	  "[perf_event][software_perf_event]")
{
	const std::string shared_memory_name =
		"SoftwarePerfEventResizeTestShm-" + std::to_string(getpid());
	const size_t shared_memory_size = 16 * 1024 * 1024;
	shm_remove remover{ std::string(shared_memory_name) };

	boost::interprocess::managed_shared_memory shm(
		boost::interprocess::create_only, shared_memory_name.c_str(),
		shared_memory_size);

	auto *perf = shm.construct<bpftime::software_perf_event_data>(
		"perf")(0, 0, 0, shm);
	REQUIRE(perf != nullptr);

	// Without a mapped ring the sample is dropped and, as in the kernel,
	// the output reports -ENOSPC.
	event_payload dropped_before_mmap{ 0, 0 };
	errno = 0;
	REQUIRE(perf->output_data(&dropped_before_mmap,
				  sizeof(dropped_before_mmap)) == -1);
	REQUIRE(errno == ENOSPC);
	REQUIRE_FALSE(perf->has_data());

	const size_t ring_size = 64;
	void *raw_buffer = perf->ensure_mmap_buffer(getpagesize() + ring_size);
	REQUIRE(raw_buffer != nullptr);

	auto *header = (perf_event_mmap_page *)raw_buffer;
	auto *base = (uint8_t *)raw_buffer + getpagesize();
	// Without 8-byte record alignment, the fourth 20-byte record would
	// start at offset 60 and split its 8-byte perf_event_header.
	for (int sequence = 0; sequence < 4; sequence++) {
		event_payload payload{ 1, sequence };
		REQUIRE(perf->output_data(&payload, sizeof(payload)) == 0);
		REQUIRE(perf->has_data());

		const uint64_t tail = header->data_tail;
		const uint64_t head = header->data_head;
		REQUIRE((tail & (ring_size - 1)) + sizeof(perf_event_header) <=
			ring_size);

		perf_event_header record_header;
		copy_from_perf_ring(base, ring_size, tail, &record_header,
				    sizeof(record_header));
		REQUIRE(record_header.type == PERF_RECORD_SAMPLE);
		REQUIRE(record_header.size ==
			aligned_perf_record_size(sizeof(event_payload)));

		event_payload actual;
		copy_from_perf_ring(base, ring_size,
				    tail + sizeof(bpftime::perf_sample_raw),
				    &actual, sizeof(actual));
		REQUIRE(actual.producer == payload.producer);
		REQUIRE(actual.sequence == payload.sequence);
		REQUIRE(tail + record_header.size == head);
		header->data_tail = head;
	}
}

TEST_CASE("Software perf event mmap reports shared memory exhaustion",
	  "[perf_event][software_perf_event]")
{
	const std::string shared_memory_name =
		"SoftwarePerfEventExhaustionTestShm-" +
		std::to_string(getpid());
	const size_t shared_memory_size = 1024 * 1024;
	shm_remove remover{ std::string(shared_memory_name) };

	boost::interprocess::managed_shared_memory shm(
		boost::interprocess::create_only, shared_memory_name.c_str(),
		shared_memory_size);

	auto *perf = shm.construct<bpftime::software_perf_event_data>(
		"perf")(0, 0, 0, shm);
	REQUIRE(perf != nullptr);

	const size_t ring_size = 2 * 1024 * 1024;
	errno = 0;
	REQUIRE(perf->ensure_mmap_buffer(getpagesize() + ring_size) == nullptr);
	REQUIRE(errno == ENOMEM);
}

namespace
{
struct large_payload {
	int sequence;
	uint8_t filler[1000];
};

std::pair<uint32_t, uint64_t> perf_record(uint32_t type, uint64_t value)
{
	return { type, value };
}

// Reads every record between tail and head and marks the ring consumed.
std::vector<std::pair<uint32_t, uint64_t>>
consume_perf_records(void *raw_buffer, size_t ring_size)
{
	auto *header = (perf_event_mmap_page *)raw_buffer;
	auto *base = (uint8_t *)raw_buffer + getpagesize();
	std::vector<std::pair<uint32_t, uint64_t>> records;
	uint64_t tail = header->data_tail;
	const uint64_t head = header->data_head;
	while (tail < head) {
		perf_event_header record_header;
		copy_from_perf_ring(base, ring_size, tail, &record_header,
				    sizeof(record_header));
		REQUIRE(record_header.size >= sizeof(record_header));
		REQUIRE(record_header.size % sizeof(uint64_t) == 0);
		if (record_header.type == PERF_RECORD_LOST) {
			// libbpf reads {header, id, lost} without sample_id.
			REQUIRE(record_header.size ==
				offsetof(bpftime::perf_sample_lost, sample_id));
			bpftime::perf_sample_lost lost;
			copy_from_perf_ring(base, ring_size, tail, &lost,
					    record_header.size);
			records.emplace_back(PERF_RECORD_LOST, lost.lost);
		} else {
			REQUIRE(record_header.type == PERF_RECORD_SAMPLE);
			REQUIRE(record_header.size ==
				aligned_perf_record_size(
					sizeof(large_payload)));
			int sequence;
			copy_from_perf_ring(
				base, ring_size,
				tail + sizeof(bpftime::perf_sample_raw),
				&sequence, sizeof(sequence));
			records.emplace_back(PERF_RECORD_SAMPLE, sequence);
		}
		tail += record_header.size;
	}
	REQUIRE(tail == head);
	header->data_tail = head;
	return records;
}
} // namespace

TEST_CASE("Software perf event reports dropped samples with PERF_RECORD_LOST",
	  "[perf_event][software_perf_event][perf-lost]")
{
	const std::string shared_memory_name =
		"SoftwarePerfEventLostTestShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(shared_memory_name) };
	boost::interprocess::managed_shared_memory shm(
		boost::interprocess::create_only, shared_memory_name.c_str(),
		16 * 1024 * 1024);
	auto *perf = shm.construct<bpftime::software_perf_event_data>(
		"perf")(0, 0, 0, shm);
	REQUIRE(perf != nullptr);

	const size_t ring_size = 4096;
	void *raw_buffer = perf->ensure_mmap_buffer(getpagesize() + ring_size);
	REQUIRE(raw_buffer != nullptr);
	constexpr size_t record_size =
		aligned_perf_record_size(sizeof(large_payload));
	const int capacity = (ring_size - 1) / record_size;
	REQUIRE(capacity == 4);
	// The space left after a full ring holds a LOST record but not a
	// sample, so a LOST record must never be published on its own.
	REQUIRE(ring_size - capacity * record_size >
		offsetof(bpftime::perf_sample_lost, sample_id));

	large_payload payload{};
	int sequence = 0;
	auto output = [&]() {
		payload.sequence = sequence++;
		errno = 0;
		return perf->output_data(&payload, sizeof(payload));
	};
	for (int i = 0; i < capacity; i++)
		REQUIRE(output() == 0);
	REQUIRE(perf->producer_shards.size() == 1);
	auto &shard_buffer = perf->producer_shards.front().buffer;
	const uint64_t full_head = shard_buffer.get_header_ref().data_head;
	constexpr int dropped = 3;
	for (int i = 0; i < dropped; i++) {
		REQUIRE(output() == -1);
		REQUIRE(errno == ENOSPC);
	}
	CHECK(shard_buffer.get_header_ref().data_head == full_head);
	CHECK(shard_buffer.lost_samples == dropped);

	REQUIRE(perf->has_data());
	auto records = consume_perf_records(raw_buffer, ring_size);
	REQUIRE(records.size() == (size_t)capacity);
	for (int i = 0; i < capacity; i++)
		CHECK(records[i] == perf_record(
					    PERF_RECORD_SAMPLE, i));

	// The next sample that fits carries the loss in front of it.
	REQUIRE(output() == 0);
	REQUIRE(output() == 0);
	CHECK(shard_buffer.lost_samples == 0);
	REQUIRE(perf->has_data());
	records = consume_perf_records(raw_buffer, ring_size);
	REQUIRE(records.size() == 3);
	CHECK(records[0] == perf_record(
				    PERF_RECORD_LOST, dropped));
	CHECK(records[1] == perf_record(
				    PERF_RECORD_SAMPLE, capacity + dropped));
	CHECK(records[2] == perf_record(
				    PERF_RECORD_SAMPLE, capacity + dropped + 1));
	REQUIRE_FALSE(perf->has_data());

	// A sample that can never fit is also counted as lost.
	std::vector<uint8_t> oversized(ring_size);
	errno = 0;
	REQUIRE(perf->output_data(oversized.data(), oversized.size()) == -1);
	CHECK(errno == ENOSPC);
	CHECK(shard_buffer.lost_samples == 1);
	REQUIRE(output() == 0);
	REQUIRE(perf->has_data());
	records = consume_perf_records(raw_buffer, ring_size);
	REQUIRE(records.size() == 2);
	CHECK(records[0] ==
	      perf_record(PERF_RECORD_LOST, 1));
	CHECK(records[1].first == PERF_RECORD_SAMPLE);
}

TEST_CASE("Software perf event drains start at a different shard each time",
	  "[perf_event][software_perf_event][perf-lost]")
{
	// A full consumer ring takes the shards drained first; always the
	// same order starved the threads behind them.
	const std::string shared_memory_name =
		"SoftwarePerfEventDrainOrderShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(shared_memory_name) };
	boost::interprocess::managed_shared_memory shm(
		boost::interprocess::create_only, shared_memory_name.c_str(),
		16 * 1024 * 1024);
	auto *perf = shm.construct<bpftime::software_perf_event_data>(
		"perf")(0, 0, 0, shm);
	const size_t ring_size = 4096; // four samples
	void *raw_buffer = perf->ensure_mmap_buffer(getpagesize() + ring_size);
	REQUIRE(raw_buffer != nullptr);
	auto fill = [&](int first) {
		large_payload payload{};
		for (int i = 0; i < 4; i++) {
			payload.sequence = first + i;
			REQUIRE(perf->output_data(&payload, sizeof(payload)) ==
				0);
		}
	};
	fill(0); // this thread's shard, the first one
	std::thread([&] { fill(100); }).join();
	REQUIRE(perf->producer_shards.size() == 2);
	int from_first = 0, from_second = 0;
	for (int round = 0; round < 2; round++) {
		REQUIRE(perf->has_data());
		for (auto &record : consume_perf_records(raw_buffer, ring_size))
			(record.second >= 100 ? from_second : from_first)++;
		if (round == 0)
			fill(4); // the first thread keeps producing
	}
	CHECK(from_first == 4);
	CHECK(from_second == 4);
}

TEST_CASE("Software perf event shards are matched within a pid namespace",
	  "[perf_event][software_perf_event]")
{
	// A producer in another pid namespace with the same pid and tid (two
	// containers' pid 1) must get its own single-producer ring.
	const std::string shared_memory_name =
		"SoftwarePerfEventNsMatchShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(shared_memory_name) };
	boost::interprocess::managed_shared_memory shm(
		boost::interprocess::create_only, shared_memory_name.c_str(),
		16 * 1024 * 1024);
	auto *perf = shm.construct<bpftime::software_perf_event_data>(
		"perf")(0, 0, 0, shm);
	REQUIRE(perf->ensure_mmap_buffer(getpagesize() + 64 * 1024) !=
		nullptr);
	struct stat st;
	REQUIRE(stat("/proc/self/ns/pid", &st) == 0);
	// Same identity as the thread below, other namespace.
	std::atomic<int64_t> tid{ 0 };
	std::thread producer([&] {
		tid = syscall(SYS_gettid);
		while (tid.load() > 0) {
		}
		event_payload payload{ 1, 0 };
		REQUIRE(perf->output_data(&payload, sizeof(payload)) == 0);
	});
	while (tid.load() == 0) {
	}
	const int64_t producer_tid = tid.load();
	perf->producer_shards.emplace_back(
		getpid(), producer_tid, 999, perf->producer_buffer_generation,
		getpagesize(), shm.get_segment_manager(),
		getpagesize() + 64 * 1024);
	perf->producer_shards.back().pid_namespace = st.st_ino + 1;
	tid = -1;
	producer.join();
	REQUIRE(perf->producer_shards.size() == 2);
	for (auto &shard : perf->producer_shards) {
		INFO("namespace " << shard.pid_namespace);
		CHECK(shard.buffer.has_data() ==
		      (shard.pid_namespace == st.st_ino));
	}
}

TEST_CASE("Software perf event shard caches are not inherited by fork children",
	  "[perf_event][software_perf_event]")
{
	// A pid 1 that forks after unshare(CLONE_NEWPID) has a pid 1 child
	// in the new namespace: the thread's cached shard (the parent's,
	// matched by pid and tid) was used without the namespace check.
	// Here the cached shard is made the other namespace's, as the
	// parent's is for that child.
	const std::string shared_memory_name =
		"SoftwarePerfEventForkCacheShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(shared_memory_name) };
	boost::interprocess::managed_shared_memory shm(
		boost::interprocess::create_only, shared_memory_name.c_str(),
		16 * 1024 * 1024);
	auto *perf = shm.construct<bpftime::software_perf_event_data>(
		"perf")(0, 0, 0, shm);
	REQUIRE(perf->ensure_mmap_buffer(getpagesize() + 64 * 1024) !=
		nullptr);
	event_payload payload{ 1, 0 };
	REQUIRE(perf->output_data(&payload, sizeof(payload)) == 0);
	REQUIRE(perf->producer_shards.size() == 1);
	perf->producer_shards.front().pid_namespace ^= 1;
	bpftime::software_perf_event_fork_child_for_test();
	REQUIRE(perf->output_data(&payload, sizeof(payload)) == 0);
	CHECK(perf->producer_shards.size() == 2);
}

TEST_CASE("Producers in two pid namespaces with the same pid write apart",
	  "[perf_event][software_perf_event]")
{
	// Two children each unshare a pid namespace and fork its pid 1; both
	// pid-1 producers write at the same time.
	if (geteuid() != 0)
		SKIP("needs root (CLONE_NEWPID)");
	const std::string shared_memory_name =
		"SoftwarePerfEventTwoNsShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(shared_memory_name) };
	boost::interprocess::managed_shared_memory shm(
		boost::interprocess::create_only, shared_memory_name.c_str(),
		16 * 1024 * 1024);
	auto *perf = shm.construct<bpftime::software_perf_event_data>(
		"perf")(0, 0, 0, shm);
	const size_t ring_size = 256 * 1024;
	void *raw_buffer = perf->ensure_mmap_buffer(getpagesize() + ring_size);
	REQUIRE(raw_buffer != nullptr);
	constexpr int per_producer = 100;
	auto *go = shm.construct<std::atomic<int>>("go")(0);
	pid_t children[2];
	for (int producer = 0; producer < 2; producer++) {
		children[producer] = fork();
		REQUIRE(children[producer] >= 0);
		if (children[producer] != 0)
			continue;
		if (unshare(CLONE_NEWPID) != 0)
			_exit(10);
		pid_t init = fork();
		if (init == 0) {
			if (getpid() != 1)
				_exit(11);
			go->fetch_add(1);
			while (go->load() < 2) {
			}
			large_payload payload{};
			for (int i = 0; i < per_producer; i++) {
				payload.sequence = producer * 1000 + i;
				if (perf->output_data(&payload,
						      sizeof(payload)) != 0)
					_exit(12);
			}
			_exit(0);
		}
		int status = -1;
		if (init < 0 || waitpid(init, &status, 0) != init)
			_exit(13);
		_exit(WIFEXITED(status) ? WEXITSTATUS(status) : 14);
	}
	for (pid_t child : children) {
		int status = -1;
		REQUIRE(waitpid(child, &status, 0) == child);
		REQUIRE(WIFEXITED(status));
		REQUIRE(WEXITSTATUS(status) == 0);
	}
	size_t pid1_shards = 0;
	for (auto &shard : perf->producer_shards)
		pid1_shards += shard.pid == 1;
	CHECK(pid1_shards == 2);
	REQUIRE(perf->has_data());
	int seen[2] = { 0, 0 };
	for (auto &record : consume_perf_records(raw_buffer, ring_size)) {
		REQUIRE(record.first == PERF_RECORD_SAMPLE);
		seen[record.second / 1000]++;
	}
	CHECK(seen[0] == per_producer);
	CHECK(seen[1] == per_producer);
}

TEST_CASE("Software perf event output fails cleanly when shared memory is full",
	  "[perf_event][software_perf_event]")
{
	// Shards of other pid namespaces are never reclaimed, so shared
	// memory can run out; Boost's bad_alloc escaped into the traced
	// process.
	const std::string shared_memory_name =
		"SoftwarePerfEventShmFullShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(shared_memory_name) };
	boost::interprocess::managed_shared_memory shm(
		boost::interprocess::create_only, shared_memory_name.c_str(),
		1024 * 1024);
	auto *perf = shm.construct<bpftime::software_perf_event_data>(
		"perf")(0, 0, 0, shm);
	REQUIRE(perf->ensure_mmap_buffer(getpagesize() + 64 * 1024) !=
		nullptr);
	struct stat st;
	REQUIRE(stat("/proc/self/ns/pid", &st) == 0);
	try {
		for (int i = 1;; i++) {
			perf->producer_shards.emplace_back(
				i, i, 1000 + i,
				perf->producer_buffer_generation,
				getpagesize(), shm.get_segment_manager(),
				getpagesize() + 64 * 1024);
			perf->producer_shards.back().pid_namespace =
				st.st_ino + 1;
		}
	} catch (const boost::interprocess::bad_alloc &) {
	}
	event_payload payload{ 1, 0 };
	errno = 0;
	int ret = -2;
	REQUIRE_NOTHROW(ret = perf->output_data(&payload, sizeof(payload)));
	CHECK(ret == -1);
	CHECK(errno == ENOSPC);
}

TEST_CASE("Software perf event output retries a shard whose data did not fit",
	  "[perf_event][software_perf_event]")
{
	// Room for a shard's list node and header page but not its 64 KiB of
	// data: the shard was kept without data, and its thread got ENOSPC
	// from then on.
	const std::string shared_memory_name =
		"SoftwarePerfEventShardDataShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(shared_memory_name) };
	boost::interprocess::managed_shared_memory shm(
		boost::interprocess::create_only, shared_memory_name.c_str(),
		1024 * 1024);
	auto *perf = shm.construct<bpftime::software_perf_event_data>(
		"perf")(0, 0, 0, shm);
	REQUIRE(perf->ensure_mmap_buffer(getpagesize() + 64 * 1024) !=
		nullptr);
	std::vector<void *> filler;
	while (shm.get_free_memory() > 24 * 1024) {
		size_t want = shm.get_free_memory() - 24 * 1024;
		if (want > 64 * 1024)
			want = 64 * 1024;
		try {
			filler.push_back(shm.allocate(want));
		} catch (const boost::interprocess::bad_alloc &) {
			break;
		}
	}
	event_payload payload{ 1, 0 };
	errno = 0;
	CHECK(perf->output_data(&payload, sizeof(payload)) == -1);
	CHECK(errno == ENOSPC);
	CHECK(perf->producer_shards.empty());
	for (void *p : filler)
		shm.deallocate(p);
	CHECK(perf->output_data(&payload, sizeof(payload)) == 0);
	CHECK(perf->producer_shards.size() == 1);
}

#if __linux__
TEST_CASE("bpf_perf_event_output returns -ENOSPC when the ring is full",
	  "[perf_event][perf-lost]")
{
	cpu_set_t allowed;
	REQUIRE(sched_getaffinity(0, sizeof(allowed), &allowed) == 0);
	const char *old_name = getenv("BPFTIME_GLOBAL_SHM_NAME");
	struct cleanup {
		bool had_name;
		std::string old_name;
		int map_fd = -1;
		int perf_fd = -1;
		~cleanup()
		{
			for (int fd : { map_fd, perf_fd }) {
				if (fd >= 0) {
					bpftime_close(fd);
					close(fd);
				}
			}
			bpftime_destroy_global_shm();
			bpftime_initialize_global_shm(
				bpftime::shm_open_type::SHM_NO_CREATE);
			if (had_name)
				setenv("BPFTIME_GLOBAL_SHM_NAME",
				       old_name.c_str(), 1);
			else
				unsetenv("BPFTIME_GLOBAL_SHM_NAME");
		}
	} cleanup{ old_name != nullptr, old_name ? old_name : "" };
	const std::string name =
		"PerfOutputEnospcTestShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(name) };
	REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
	bpftime_destroy_global_shm();
	bpftime_initialize_global_shm(
		bpftime::shm_open_type::SHM_REMOVE_AND_CREATE);

	cleanup.map_fd = bpftime_maps_create(
		-1, "perf_array",
		{ .type = BPF_MAP_TYPE_PERF_EVENT_ARRAY,
		  .key_size = 4,
		  .value_size = 4,
		  .max_ents = CPU_SETSIZE });
	REQUIRE(cleanup.map_fd >= 0);
	cleanup.perf_fd = bpftime_add_software_perf_event(0, 0, 0);
	REQUIRE(cleanup.perf_fd >= 0);
	REQUIRE(bpftime_get_software_perf_event_raw_buffer(
			cleanup.perf_fd, 2 * getpagesize()) != nullptr);
	for (int cpu = 0; cpu < CPU_SETSIZE; cpu++) {
		if (CPU_ISSET(cpu, &allowed))
			REQUIRE(bpftime_map_update_elem(cleanup.map_fd, &cpu,
							&cleanup.perf_fd,
							0) == 0);
	}

	std::vector<uint8_t> payload(getpagesize() / 4);
	int accepted = 0;
	uint64_t ret = 0;
	while (accepted < 16) {
		ret = bpf_perf_event_output(
			0, cleanup.map_fd, BPF_F_CURRENT_CPU,
			reinterpret_cast<uint64_t>(payload.data()),
			payload.size());
		if (ret != 0)
			break;
		accepted++;
	}
	CHECK(accepted == 3);
	CHECK((int64_t)ret == -ENOSPC);
}
#endif

TEST_CASE("Software perf event asks for a wakeup at half a ring or on a drop",
	  "[perf_event][software_perf_event][perf-wakeup]")
{
	const std::string shared_memory_name =
		"SoftwarePerfEventWatermarkTestShm-" + std::to_string(getpid());
	shm_remove remover{ std::string(shared_memory_name) };
	boost::interprocess::managed_shared_memory shm(
		boost::interprocess::create_only, shared_memory_name.c_str(),
		16 * 1024 * 1024);
	auto *perf = shm.construct<bpftime::software_perf_event_data>(
		"perf")(0, 0, 0, shm);
	REQUIRE(perf != nullptr);

	large_payload payload{};
	bool wakeup = true;
	REQUIRE(perf->output_data(&payload, sizeof(payload), &wakeup) == -1);
	CHECK_FALSE(wakeup); // No ring: nobody to wake.

	const size_t ring_size = 4096;
	void *raw_buffer = perf->ensure_mmap_buffer(getpagesize() + ring_size);
	REQUIRE(raw_buffer != nullptr);
	constexpr size_t record_size =
		aligned_perf_record_size(sizeof(large_payload));
	// 1016-byte records: half of the ring is reached by the third one.
	const bool expected[] = { false, false, true, true };
	for (bool expect : expected) {
		REQUIRE(perf->output_data(&payload, sizeof(payload), &wakeup) ==
			0);
		CHECK(wakeup == expect);
	}
	REQUIRE(perf->output_data(&payload, sizeof(payload), &wakeup) == -1);
	CHECK(wakeup);

	// Draining resets the fill level; the LOST record counts toward it.
	REQUIRE(perf->has_data());
	consume_perf_records(raw_buffer, ring_size);
	REQUIRE(perf->output_data(&payload, sizeof(payload), &wakeup) == 0);
	CHECK_FALSE(wakeup);
	REQUIRE(record_size + offsetof(bpftime::perf_sample_lost, sample_id) <
		ring_size / 2);
}

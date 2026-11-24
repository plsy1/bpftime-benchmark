#include "bpf_map/userspace/stack_trace_map.hpp"
#include "catch2/catch_test_macros.hpp"
#include "spdlog/cfg/env.h"
#include "spdlog/spdlog.h"
#include "unit-test/common_def.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <random>
#include <thread>
#include <vector>
static const char *SHM_NAME = "BPFTIME_TEST_STACK_TRACE_MAP";
using namespace bpftime;
using namespace boost::interprocess;

TEST_CASE("Test stack trace map 1")
{
	shm_remove remover(SHM_NAME);
	managed_shared_memory mem(create_only, SHM_NAME, 20 << 20);

	stack_trace_map_impl impl(mem, 4, 255 * 8, 10000);
	std::vector<uint64_t> test_stack{ 1, 2, 3, 4, 5 };

	int id1 = impl.fill_stack_trace(test_stack, false, false);
	REQUIRE(id1 >= 0);
	{
		uint64_t *query_result = (uint64_t *)impl.elem_lookup(&id1);
		REQUIRE(query_result != nullptr);
		for (size_t i = 0; i < test_stack.size(); i++)
			REQUIRE(query_result[i] == test_stack[i]);
	}
	REQUIRE(impl.elem_delete(&id1) == 0);
	REQUIRE((impl.elem_lookup(&id1) == nullptr && errno == ENOENT));
}
static std::map<int, std::vector<std::vector<uint64_t> > >
make_collision(int max_entries, int total_count, int vector_size)
{
	std::mt19937 gen(12345);
	std::uniform_int_distribution<uint32_t> rand(1, 1000);
	std::map<int, std::vector<std::vector<uint64_t> > > result;
	for (int i = 0; i < total_count; i++) {
		while (true) {
			std::vector<uint64_t> curr;
			for (int j = 0; j < vector_size; j++) {
				curr.push_back(rand(gen));
			}
			int hash = stack_trace_map::hash_stack_trace(curr) %
				   max_entries;
			result[hash].push_back(curr);
			if (result[hash].size() >= 5)
				break;
		}
	}
	return result;
}
TEST_CASE("Test stack trace map 2")
{
	spdlog::cfg::load_env_levels();

	shm_remove remover(SHM_NAME);
	managed_shared_memory mem(create_only, SHM_NAME, 20 << 20);

	stack_trace_map_impl impl(mem, 4, 255 * 8, 1000);

	auto generated = make_collision(1000, 10000, 3);
	{
		bool has_collision = false;
		for (const auto &item : generated)
			if (item.second.size() > 1)
				has_collision = true;
		REQUIRE(has_collision);
	}
	auto stk1 = generated[100].at(0);
	int id1 = impl.fill_stack_trace(stk1, false, false);
	REQUIRE(id1 >= 0);
	// Make some collision
	auto stk2 = generated[100].at(1);

	// Same bucket, different trace and full hash: the kernel returns
	// -EEXIST without BPF_F_REUSE_STACKID, also with BPF_F_FAST_STACK_CMP.
	REQUIRE(impl.fill_stack_trace(stk2, false, true) == -EEXIST);
	REQUIRE(impl.fill_stack_trace(stk2, false, false) == -EEXIST);
	// The stored trace is still stk1
	{
		auto result = (uint64_t *)impl.elem_lookup(&id1);
		REQUIRE(std::equal(stk1.begin(), stk1.end(), result));
	}
	// BPF_F_REUSE_STACKID replaces it
	REQUIRE(impl.fill_stack_trace(stk2, true, false) == id1);
	{
		auto result = (uint64_t *)impl.elem_lookup(&id1);
		REQUIRE(std::equal(stk2.begin(), stk2.end(), result));
	}
	SPDLOG_INFO("Checking insertion for two same stack trace..");
	// Put two same entries into the map
	auto stk3 = generated[233].at(2);
	int id3 = impl.fill_stack_trace(stk3, true, false);

	auto stk4 = generated[233].at(2);
	int id4 = impl.fill_stack_trace(stk4, true, false);

	// The query result should be the same as id4
	{
		REQUIRE(id3 == id4);
		auto result = (uint64_t *)impl.elem_lookup(&id3);
		REQUIRE(std::equal(stk4.begin(), stk4.end(), result));
	}
}

// Search for two distinct stacks (with the given sizes) that hash into the
// same bucket. The search is deterministic through a fixed seed.
static bool
find_colliding_stacks(uint32_t max_entries, size_t size_a, size_t size_b,
		      std::vector<uint64_t> &stk_a,
		      std::vector<uint64_t> &stk_b, uint32_t seed)
{
	std::mt19937 gen(seed);
	std::uniform_int_distribution<uint64_t> rand(1, 1000);
	auto make_stack = [&](size_t n) {
		std::vector<uint64_t> v(n);
		for (auto &x : v)
			x = rand(gen);
		return v;
	};
	std::map<uint32_t, std::vector<uint64_t>> occupied;
	for (int i = 0; i < 20000; i++) {
		auto v = make_stack(size_a);
		occupied.emplace(
			stack_trace_map::hash_stack_trace(v) % max_entries, v);
	}
	for (int i = 0; i < 20000; i++) {
		auto v = make_stack(size_b);
		uint32_t bucket =
			stack_trace_map::hash_stack_trace(v) % max_entries;
		auto it = occupied.find(bucket);
		if (it != occupied.end() && it->second != v) {
			stk_a = it->second;
			stk_b = v;
			return true;
		}
	}
	return false;
}

TEST_CASE("Test stack trace map 3: equal-size different-content collision")
{
	shm_remove remover(SHM_NAME);
	managed_shared_memory mem(create_only, SHM_NAME, 20 << 20);
	stack_trace_map_impl impl(mem, 4, 255 * 8, 1000);

	std::vector<uint64_t> stk1, stk2;
	REQUIRE(find_colliding_stacks(1000, 3, 3, stk1, stk2, 0x1234));
	REQUIRE(stk1.size() == stk2.size());
	REQUIRE(stk1 != stk2);
	REQUIRE(stack_trace_map::hash_stack_trace(stk1) % 1000 ==
		stack_trace_map::hash_stack_trace(stk2) % 1000);

	int id1 = impl.fill_stack_trace(stk1, false, false);
	REQUIRE(id1 >= 0);

	// Kernel semantics: without BPF_F_REUSE_STACKID, a collision with a
	// different stack returns -EEXIST and the original stack is kept.
	int id2 = impl.fill_stack_trace(stk2, false, false);
	REQUIRE(id2 == -EEXIST);
	{
		auto result = (uint64_t *)impl.elem_lookup(&id1);
		REQUIRE(result != nullptr);
		REQUIRE(std::equal(stk1.begin(), stk1.end(), result));
	}

	// With BPF_F_REUSE_STACKID the id is reused and the stack replaced.
	int id3 = impl.fill_stack_trace(stk2, true, false);
	REQUIRE(id3 == id1);
	{
		auto result = (uint64_t *)impl.elem_lookup(&id3);
		REQUIRE(result != nullptr);
		REQUIRE(std::equal(stk2.begin(), stk2.end(), result));
	}

	// Inserting the very same stack again deduplicates to the same id
	// instead of reporting a collision.
	int id4 = impl.fill_stack_trace(stk2, false, false);
	REQUIRE(id4 == id3);
}

TEST_CASE("Test stack trace map 4: different-size collision")
{
	shm_remove remover(SHM_NAME);
	managed_shared_memory mem(create_only, SHM_NAME, 20 << 20);
	stack_trace_map_impl impl(mem, 4, 255 * 8, 1000);

	std::vector<uint64_t> stk1, stk2;
	REQUIRE(find_colliding_stacks(1000, 3, 5, stk1, stk2, 0x5678));
	REQUIRE(stk1.size() != stk2.size());
	REQUIRE(stack_trace_map::hash_stack_trace(stk1) % 1000 ==
		stack_trace_map::hash_stack_trace(stk2) % 1000);

	int id1 = impl.fill_stack_trace(stk1, false, false);
	REQUIRE(id1 >= 0);

	int id2 = impl.fill_stack_trace(stk2, false, false);
	REQUIRE(id2 == -EEXIST);
	{
		auto result = (uint64_t *)impl.elem_lookup(&id1);
		REQUIRE(result != nullptr);
		REQUIRE(std::equal(stk1.begin(), stk1.end(), result));
	}

	int id3 = impl.fill_stack_trace(stk2, true, false);
	REQUIRE(id3 == id1);
	{
		auto result = (uint64_t *)impl.elem_lookup(&id3);
		REQUIRE(result != nullptr);
		REQUIRE(std::equal(stk2.begin(), stk2.end(), result));
	}
}

TEST_CASE("Test stack trace map 5: fast hash comparison keeps legacy id reuse")
{
	shm_remove remover(SHM_NAME);
	managed_shared_memory mem(create_only, SHM_NAME, 20 << 20);
	stack_trace_map_impl impl(mem, 4, 255 * 8, 1000);

	std::vector<uint64_t> stk1, stk2;
	REQUIRE(find_colliding_stacks(1000, 3, 3, stk1, stk2, 0x9abc));
	REQUIRE(stk1 != stk2);

	int id1 = impl.fill_stack_trace(stk1, false, false);
	REQUIRE(id1 >= 0);

	// BPF_F_FAST_STACK_CMP compares by hash/bucket only: distinct stacks
	// still share the id and never trigger -EEXIST.
	int id2 = impl.fill_stack_trace(stk2, false, true);
	REQUIRE(id2 == id1);
	int id3 = impl.fill_stack_trace(stk2, true, true);
	REQUIRE(id3 == id1);
}

TEST_CASE("Stack trace map pads, truncates and compares like the kernel",
	  "[maps][stack_trace]")
{
	shm_remove remover(SHM_NAME);
	managed_shared_memory mem(create_only, SHM_NAME, 20 << 20);
	// value_size of 4 frames
	stack_trace_map_impl impl(mem, 4, 4 * 8, 1000);

	// A short trace reads back zero-padded to the whole value: lookups
	// copy value_size bytes.
	std::vector<uint64_t> short_stack{ 0x1000, 0x2000 };
	int id = impl.fill_stack_trace(short_stack, false, false);
	REQUIRE(id >= 0);
	auto value = (const uint64_t *)impl.elem_lookup(&id);
	REQUIRE(value[0] == 0x1000);
	REQUIRE(value[1] == 0x2000);
	REQUIRE(value[2] == 0);
	REQUIRE(value[3] == 0);
	REQUIRE(impl.fill_stack_trace(short_stack, false, false) == id);

	// Frames beyond value_size are dropped; the result equals the trace
	// of the first four frames.
	std::vector<uint64_t> long_stack{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };
	std::vector<uint64_t> first_four{ 0x10, 0x20, 0x30, 0x40 };
	int long_id = impl.fill_stack_trace(long_stack, false, false);
	REQUIRE(long_id >= 0);
	REQUIRE(impl.fill_stack_trace(first_four, false, false) == long_id);
	value = (const uint64_t *)impl.elem_lookup(&long_id);
	REQUIRE(std::equal(first_four.begin(), first_four.end(), value));
}

TEST_CASE("Stack trace lookups without the lock find stored traces only",
	  "[maps][stack_trace]")
{
	shm_remove remover(SHM_NAME);
	managed_shared_memory mem(create_only, SHM_NAME, 20 << 20);
	// One bucket: every trace has id 0.
	stack_trace_map_impl impl(mem, 4, 4 * 8, 1);
	std::vector<uint64_t> a{ 0x10, 0x20, 0x30 };
	std::vector<uint64_t> b{ 0x10, 0x20, 0x40 };

	REQUIRE(impl.find_stack_trace(a.data(), a.size(), false) == -1);
	REQUIRE(impl.fill_stack_trace(a, false, false) == 0);
	REQUIRE(impl.find_stack_trace(a.data(), a.size(), false) == 0);
	REQUIRE(impl.find_stack_trace(a.data(), a.size(), true) == 0);
	// Another trace in the slot: the locked fill decides (-EEXIST here).
	REQUIRE(impl.find_stack_trace(b.data(), b.size(), false) == -1);
	REQUIRE(impl.find_stack_trace(b.data(), b.size(), true) == -1);
	REQUIRE(impl.fill_stack_trace(b, false, false) == -EEXIST);
	// Frames beyond the value size are dropped before the comparison.
	std::vector<uint64_t> a_long{ 0x10, 0x20, 0x30, 0x50, 0x60 };
	std::vector<uint64_t> a_four{ 0x10, 0x20, 0x30, 0x50 };
	REQUIRE(impl.fill_stack_trace(a_four, true, false) == 0);
	REQUIRE(impl.find_stack_trace(a_long.data(), a_long.size(), false) ==
		0);
	// A shorter replacement reads back zero-padded.
	REQUIRE(impl.fill_stack_trace(a, true, false) == 0);
	int id = 0;
	auto value = (const uint64_t *)impl.elem_lookup(&id);
	REQUIRE(value != nullptr);
	REQUIRE(value[3] == 0);
	// A deleted trace is not found, also by its hash.
	REQUIRE(impl.elem_delete(&id) == 0);
	REQUIRE(impl.find_stack_trace(a.data(), a.size(), false) == -1);
	REQUIRE(impl.find_stack_trace(a.data(), a.size(), true) == -1);
	REQUIRE((impl.elem_lookup(&id) == nullptr && errno == ENOENT));
	REQUIRE(impl.fill_stack_trace(b, false, false) == 0);
	// No frame: -EFAULT, as the kernel when the skip leaves none.
	REQUIRE(impl.fill_stack_trace(std::vector<uint64_t>{}, false, false) ==
		-EFAULT);
	REQUIRE(impl.find_stack_trace(nullptr, 0, false) == -1);
	int out_of_range = 1;
	REQUIRE((impl.elem_lookup(&out_of_range) == nullptr &&
		 errno == ENOENT));
}

// A and B differ in the first and the last frame; H has A's first and B's
// last. Readers without the lock look H up while a writer replaces A with B
// and back: a reader whose first frame load sees A and last sees B has H's
// frames, so only the hash keeps H from matching (hash(H) is never stored).
TEST_CASE("Stack trace lookups without the lock never take a mix of two "
	  "traces for a third",
	  "[maps][stack_trace]")
{
	shm_remove remover(SHM_NAME);
	managed_shared_memory mem(create_only, SHM_NAME, 20 << 20);
	constexpr size_t frames = 127;
	stack_trace_map_impl impl(mem, 4, frames * 8, 1);
	std::vector<uint64_t> a(frames);
	for (size_t i = 0; i < frames; i++)
		a[i] = 0x400000 + i * 0x10;
	std::vector<uint64_t> b = a;
	b[0] = 0x500000;
	b[frames - 1] = 0x5000f0;
	std::vector<uint64_t> h = a;
	h[frames - 1] = b[frames - 1];
	REQUIRE(impl.fill_stack_trace(a, false, false) == 0);

	std::mutex lock;
	std::atomic<bool> stop{ false };
	std::atomic<uint64_t> wrong{ 0 }, reads{ 0 }, writes{ 0 };
	std::thread writer([&] {
		while (!stop.load(std::memory_order_relaxed)) {
			{
				std::lock_guard guard(lock);
				impl.fill_stack_trace(b, true, false);
			}
			{
				std::lock_guard guard(lock);
				impl.fill_stack_trace(a, true, false);
			}
			writes.fetch_add(2, std::memory_order_relaxed);
		}
	});
	std::vector<std::thread> readers;
	for (int t = 0; t < 4; t++)
		readers.emplace_back([&] {
			uint64_t local_wrong = 0, local_reads = 0;
			while (!stop.load(std::memory_order_relaxed)) {
				if (impl.find_stack_trace(h.data(), h.size(),
							  false) >= 0)
					local_wrong++;
				local_reads++;
			}
			wrong.fetch_add(local_wrong);
			reads.fetch_add(local_reads);
		});
	std::this_thread::sleep_for(std::chrono::seconds(2));
	stop.store(true);
	writer.join();
	for (auto &t : readers)
		t.join();
	INFO("reads " << reads.load() << ", writes " << writes.load());
	REQUIRE(reads.load() > 0);
	REQUIRE(writes.load() > 0);
	REQUIRE(wrong.load() == 0);
	std::lock_guard guard(lock);
	REQUIRE(impl.fill_stack_trace(h, false, false) == -EEXIST);
}

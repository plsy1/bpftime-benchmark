// Per-CPU hash maps shared by threads: programs on several threads insert,
// look up and delete keys while the loader iterates the map. The map's shared
// state (the key index, the value slot and node free lists, the key scratch
// buffers) used to be modified without the map lock, so concurrent inserts
// corrupted it and crashed the traced process.
#include "handler/map_handler.hpp"
#include <atomic>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <linux/bpf.h>
#include <string>
#include <thread>
#include <vector>

using namespace boost::interprocess;
using namespace bpftime;

namespace
{
// Workers plus the loader thread stay below the CPU count: a spinning thread
// that preempts the lock holder made runs take tens of seconds.
constexpr int threads = 4;
constexpr int rounds = 40;
constexpr uint32_t keys_per_thread = 300;

template <typename Key> Key make_key(int thread, uint32_t i)
{
	Key key{};
	const uint32_t value = (uint32_t)thread * 100000 + i;
	std::memcpy(&key, &value, sizeof(value));
	return key;
}

// Each thread inserts its own keys (typed helper entry point, then the generic
// one), checks that they are all visible, then deletes them; a loader thread
// walks the map with get_next_key the whole time. Returns the number of
// inconsistencies seen (missing keys, failed inserts or deletes).
template <typename Key> uint64_t hammer(bpf_map_handler &map)
{
	std::atomic<uint64_t> errors{ 0 };
	std::atomic<bool> done{ false };
	std::vector<std::thread> workers;
	for (int t = 0; t < threads; t++) {
		workers.emplace_back([&, t] {
			uint64_t value = 1;
			for (int r = 0; r < rounds; r++) {
				for (uint32_t i = 0; i < keys_per_thread; i++) {
					Key key = make_key<Key>(t, i);
					long ret =
						i % 2 ? map.map_update_percpu_hash(
								&key, &value,
								BPF_NOEXIST) :
							map.map_update_elem(
								&key, &value,
								BPF_NOEXIST,
								false);
					if (ret != 0)
						errors++;
				}
				for (uint32_t i = 0; i < keys_per_thread; i++) {
					Key key = make_key<Key>(t, i);
					const void *v =
						i % 2 ? map.map_lookup_percpu_hash(
								&key) :
							map.map_lookup_elem(
								&key, false);
					if (v == nullptr)
						errors++;
				}
				for (uint32_t i = 0; i < keys_per_thread; i++) {
					Key key = make_key<Key>(t, i);
					if (map.map_delete_elem(&key, false) != 0)
						errors++;
				}
			}
		});
	}
	std::thread loader([&] {
		Key key, next;
		while (!done.load()) {
			const void *prev = nullptr;
			int steps = 0;
			while (map.bpf_map_get_next_key(prev, &next, true) == 0 &&
			       steps++ < 100000) {
				key = next;
				prev = &key;
			}
		}
	});
	for (auto &w : workers)
		w.join();
	done = true;
	loader.join();
	// Every key was deleted again.
	Key next;
	if (map.bpf_map_get_next_key(nullptr, &next, true) == 0)
		errors++;
	return errors.load();
}
} // namespace

TEST_CASE("per-cpu hash stays consistent under concurrent inserts and deletes",
	  "[percpu-hash][concurrency]")
{
	const char *SHM_NAME = "BPFTIME_TEST_PER_CPU_HASH_CONCURRENCY";
	shared_memory_object::remove(SHM_NAME);
	managed_shared_memory segment(create_only, SHM_NAME, 64 << 20);
	const uint32_t max_entries = threads * keys_per_thread;

	SECTION("four-byte keys")
	{
		bpf_map_handler map(1, BPF_MAP_TYPE_PERCPU_HASH, 4, 8,
				    max_entries, 0, "pcpu_hash_k4", segment);
		REQUIRE(map.map_init(segment) == 0);
		REQUIRE(hammer<uint32_t>(map) == 0);
		map.map_free(segment);
	}
	SECTION("eight-byte keys")
	{
		bpf_map_handler map(2, BPF_MAP_TYPE_PERCPU_HASH, 8, 8,
				    max_entries, 0, "pcpu_hash_k8", segment);
		REQUIRE(map.map_init(segment) == 0);
		REQUIRE(hammer<uint64_t>(map) == 0);
		map.map_free(segment);
	}
	shared_memory_object::remove(SHM_NAME);
}

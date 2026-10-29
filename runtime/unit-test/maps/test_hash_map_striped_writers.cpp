// Hash map writers lock stripes of buckets instead of the whole map. Threads
// own disjoint keys and check every result against a private reference; the
// keys are chosen with clustered hashes (the hasher weighs the last two
// bytes of a four-byte key by 31 and 1), so probe runs are long, cross
// stripe boundaries and wrap past the end of the table. A writer that locked
// too few stripes would let two inserts take the same bucket, or a delete
// release a bucket another key's probe path still runs through.
#include <bpf_map/bpftime_hash_map.hpp>
#include <bpf_map/userspace/fix_hash_map.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <linux/bpf.h>
#include <map>
#include <random>
#include <sched.h>
#include <set>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace boost::interprocess;
using namespace bpftime;

namespace
{
struct shm_guard {
	const char *name;
	managed_shared_memory segment;
	explicit shm_guard(const char *name)
		: name((shared_memory_object::remove(name), name)),
		  segment(create_only, name, 64 << 20)
	{
	}
	~shm_guard()
	{
		shared_memory_object::remove(name);
	}
};

// Keys whose hash is b2 * 31 + b3 (b0 = b1 = 0): 3600 keys on about 1800
// home buckets, so each insert probes past several others.
uint32_t clustered_key(uint32_t n)
{
	const uint32_t b2 = (n / 64) & 0xff, b3 = (n % 64) & 0xff;
	return (b2 << 16) | (b3 << 24);
}

size_t home_of(const bpftime_hash_map &map, uint32_t key)
{
	return bpftime_hasher::hash_func(&key, sizeof(key)) % map.bucket_count();
}
} // namespace

TEST_CASE("striped hash writers agree with a reference under contention",
	  "[bpftime_hash_map][concurrency]")
{
	shm_guard shm("BPFTIME_TEST_HASH_STRIPED_REFERENCE");
	// 4096 entries: 8209 buckets in 129 stripes of 64; the clustered keys
	// fill a dense run of about 30 stripes.
	constexpr size_t max_entries = 4096;
	constexpr int threads = 4;
	constexpr int keys_per_thread = 900;
	constexpr int ops_per_thread = 200000;
	fix_size_hash_map_impl map(shm.segment, max_entries, 4, 8);

	std::atomic<uint64_t> errors{ 0 };
	std::vector<std::map<uint32_t, uint64_t>> refs(threads);
	std::vector<std::thread> workers;
	for (int t = 0; t < threads; t++)
		workers.emplace_back([&, t] {
			std::mt19937 rng(1234 + t);
			auto &ref = refs[t];
			uint64_t bad = 0;
			for (int op = 0; op < ops_per_thread; op++) {
				// Interleave the threads' keys over the cluster.
				const uint32_t key = clustered_key(
					(uint32_t)(rng() % keys_per_thread) *
						threads +
					t);
				const uint64_t value = ((uint64_t)t << 32) | op;
				const auto it = ref.find(key);
				switch (rng() % 4) {
				case 0: { // insert
					const long r = map.elem_update(
						&key, &value, BPF_NOEXIST);
					if (it == ref.end()) {
						bad += r != 0;
						ref[key] = value;
					} else {
						bad += !(r == -1 && errno == EEXIST);
					}
					break;
				}
				case 1: { // update an existing key
					const long r = map.elem_update(&key, &value,
								       BPF_EXIST);
					if (it == ref.end()) {
						bad += !(r == -1 && errno == ENOENT);
					} else {
						bad += r != 0;
						it->second = value;
					}
					break;
				}
				case 2: { // delete
					const long r = map.elem_delete(&key);
					if (it == ref.end()) {
						bad += !(r == -1 && errno == ENOENT);
					} else {
						bad += r != 0;
						ref.erase(it);
					}
					break;
				}
				default: { // lookup
					const auto *v = static_cast<const uint64_t *>(
						map.elem_lookup(&key));
					if (it == ref.end())
						bad += v != nullptr;
					else
						bad += v == nullptr || *v != it->second;
				}
				}
			}
			errors += bad;
		});
	for (auto &w : workers)
		w.join();
	REQUIRE(errors.load() == 0);

	// Every key exactly once, with its last value.
	std::map<uint32_t, uint64_t> expected;
	for (auto &ref : refs)
		expected.insert(ref.begin(), ref.end());
	std::set<uint32_t> seen;
	uint32_t key, next;
	const void *prev = nullptr;
	while (map.map_get_next_key(prev, &next) == 0) {
		REQUIRE(seen.insert(next).second);
		key = next;
		prev = &key;
	}
	REQUIRE(seen.size() == expected.size());
	for (auto &[k, v] : expected) {
		const auto *value =
			static_cast<const uint64_t *>(map.elem_lookup(&k));
		REQUIRE(value != nullptr);
		REQUIRE(*value == v);
	}
}

TEST_CASE("striped hash map keeps max_entries exact across threads",
	  "[bpftime_hash_map][concurrency]")
{
	shm_guard shm("BPFTIME_TEST_HASH_STRIPED_CAPACITY");
	constexpr size_t max_entries = 3000;
	constexpr int threads = 4;
	bpftime_hash_map map(shm.segment, max_entries, 4, 8);
	auto fill = [&](uint32_t base) {
		std::atomic<uint64_t> failed{ 0 };
		std::vector<std::thread> workers;
		for (int t = 0; t < threads; t++)
			workers.emplace_back([&, t] {
				for (uint32_t i = t; i < max_entries; i += threads) {
					const uint32_t key = base + i;
					const uint64_t value = key;
					if (!map.elem_update(&key, &value))
						failed++;
				}
			});
		for (auto &w : workers)
			w.join();
		return failed.load();
	};
	REQUIRE(fill(0) == 0);
	REQUIRE(map.get_elem_count() == max_entries);
	const uint32_t extra = 1u << 30;
	const uint64_t value = 0;
	REQUIRE_FALSE(map.elem_update(&extra, &value));
	// Deleting on other threads leaves the tokens spread over the stripes
	// (and the pool); the refill must still reach max_entries.
	{
		std::atomic<uint64_t> failed{ 0 };
		std::vector<std::thread> workers;
		for (int t = 0; t < threads; t++)
			workers.emplace_back([&, t] {
				for (uint32_t i = t; i < max_entries; i += threads)
					if (!map.elem_delete(&i))
						failed++;
			});
		for (auto &w : workers)
			w.join();
		REQUIRE(failed.load() == 0);
	}
	REQUIRE(map.get_elem_count() == 0);
	REQUIRE(fill(100000) == 0);
	REQUIRE(map.get_elem_count() == max_entries);
	REQUIRE_FALSE(map.elem_update(&extra, &value));
}

TEST_CASE("striped hash map probe runs wrap past the last stripe",
	  "[bpftime_hash_map][concurrency]")
{
	shm_guard shm("BPFTIME_TEST_HASH_STRIPED_WRAP");
	bpftime_hash_map map(shm.segment, 256, 4, 8);
	REQUIRE(map.stripe_count() > 2);
	const size_t last = map.bucket_count() - 1;
	// Keys whose home is the last bucket run into stripe 0; keys with home
	// bucket 0 compete with them for stripe 0.
	std::vector<uint32_t> at_end, at_start;
	for (uint32_t key = 1; (at_end.size() < 100 || at_start.size() < 100) &&
			       key < (1u << 26);
	     key++) {
		const size_t home = home_of(map, key);
		if (home == last && at_end.size() < 100)
			at_end.push_back(key);
		else if (home == 0 && at_start.size() < 100)
			at_start.push_back(key);
	}
	REQUIRE(at_end.size() == 100);
	REQUIRE(at_start.size() == 100);
	std::atomic<uint64_t> errors{ 0 };
	auto churn = [&](const std::vector<uint32_t> &keys) {
		uint64_t bad = 0;
		for (int round = 0; round < 300; round++) {
			for (uint32_t key : keys) {
				const uint64_t value = key + round;
				bad += !map.elem_update(&key, &value);
			}
			for (uint32_t key : keys) {
				const auto *v = static_cast<const uint64_t *>(
					map.elem_lookup(&key));
				bad += v == nullptr || *v != key + round;
			}
			for (size_t i = round % 2; i < keys.size(); i += 2)
				bad += !map.elem_delete(&keys[i]);
			for (size_t i = round % 2; i < keys.size(); i += 2)
				bad += map.elem_lookup(&keys[i]) != nullptr;
		}
		errors += bad;
	};
	std::thread a(churn, std::cref(at_end));
	std::thread b(churn, std::cref(at_start));
	a.join();
	b.join();
	REQUIRE(errors.load() == 0);
	// The end keys wrapped: some sit before the last bucket's index.
	size_t wrapped = 0;
	for (uint32_t key : at_end) {
		const auto *v = map.elem_lookup(&key);
		if (v != nullptr && map.get_index_of_value(v) < last)
			wrapped++;
	}
	REQUIRE(wrapped > 0);
}

TEST_CASE("striped hash map is shared by processes mapping it elsewhere",
	  "[bpftime_hash_map][concurrency]")
{
	// Each child maps the segment again, at an address of its own, and works
	// on the map with the others: buckets, stripe locks, probe bounds and
	// the token pool all live in the segment. First disjoint clustered keys
	// against private references, then every process inserts until E2BIG.
	static const char *name = "BPFTIME_TEST_HASH_STRIPED_PROCESSES";
	shm_guard shm(name);
	constexpr size_t max_entries = 4096;
	constexpr int procs = 4;
	constexpr int keys_per_proc = 900;
	constexpr int ops_per_proc = 100000;
	struct shared_state {
		std::atomic<int> started{ 0 };
		std::atomic<int> referenced{ 0 };
		std::atomic<uint64_t> errors{ 0 };
		std::atomic<uint64_t> inserted{ 0 };
	};
	auto *map = shm.segment.construct<fix_size_hash_map_impl>("map")(
		shm.segment, max_entries, 4, 8);
	shm.segment.construct<shared_state>("state")();

	// Child body; returns the exit status (never REQUIRE in a child).
	auto child = [&](uint32_t p) -> int {
		managed_shared_memory segment(open_only, name);
		auto *m = segment.find<fix_size_hash_map_impl>("map").first;
		auto *s = segment.find<shared_state>("state").first;
		if (m == nullptr || s == nullptr)
			return 2;
		if (m == map)
			return 3; // Not mapped elsewhere: the test would prove nothing.
		auto barrier = [&](std::atomic<int> &count) {
			count++;
			const auto deadline = std::chrono::steady_clock::now() +
					      std::chrono::seconds(60);
			while (count.load() < procs)
				if (std::chrono::steady_clock::now() > deadline)
					return false;
				else
					sched_yield();
			return true;
		};
		if (!barrier(s->started))
			return 4;
		std::mt19937 rng(4321 + p);
		std::map<uint32_t, uint64_t> ref;
		uint64_t bad = 0;
		for (int op = 0; op < ops_per_proc; op++) {
			const uint32_t key = clustered_key(
				(uint32_t)(rng() % keys_per_proc) * procs + p);
			const uint64_t value = ((uint64_t)p << 32) | op;
			const auto it = ref.find(key);
			if (rng() % 3 == 0) {
				const long r = m->elem_delete(&key);
				if (it == ref.end()) {
					bad += !(r == -1 && errno == ENOENT);
				} else {
					bad += r != 0;
					ref.erase(it);
				}
			} else if (rng() % 2 == 0) {
				bad += m->elem_update(&key, &value, BPF_ANY) != 0;
				ref[key] = value;
			} else {
				const auto *v = static_cast<const uint64_t *>(
					m->elem_lookup(&key));
				if (it == ref.end())
					bad += v != nullptr;
				else
					bad += v == nullptr || *v != it->second;
			}
		}
		// Known final state: every third key kept, with value key * 7.
		for (uint32_t i = 0; i < keys_per_proc; i++) {
			const uint32_t key = clustered_key(i * procs + p);
			if (i % 3 == 0) {
				const uint64_t value = (uint64_t)key * 7;
				bad += m->elem_update(&key, &value, BPF_ANY) != 0;
			} else {
				const long r = m->elem_delete(&key);
				bad += r != 0 && errno != ENOENT;
			}
		}
		s->errors += bad;
		if (!barrier(s->referenced))
			return 5;
		// Fill: keys with nonzero low 16 bits, unlike the clustered ones.
		bad = 0;
		for (uint32_t i = 0;; i++) {
			const uint32_t key = 1 + i + (p << 13);
			const uint64_t value = key;
			if (m->elem_update(&key, &value, BPF_NOEXIST) == 0) {
				s->inserted++;
				continue;
			}
			bad += errno != E2BIG;
			break;
		}
		s->errors += bad;
		return 0;
	};
	std::vector<pid_t> pids;
	for (uint32_t p = 0; p < procs; p++) {
		const pid_t pid = fork();
		if (pid == 0)
			_exit(child(p));
		REQUIRE(pid > 0);
		pids.push_back(pid);
	}
	for (pid_t pid : pids) {
		int status = 0;
		REQUIRE(waitpid(pid, &status, 0) == pid);
		REQUIRE(WIFEXITED(status));
		REQUIRE(WEXITSTATUS(status) == 0);
	}
	auto *state = shm.segment.find<shared_state>("state").first;
	REQUIRE(state->errors.load() == 0);
	// Exactly max_entries in all: kept keys plus the fill.
	constexpr size_t kept = procs * keys_per_proc / 3;
	REQUIRE(state->inserted.load() == max_entries - kept);
	std::set<uint32_t> seen;
	uint32_t key, next;
	const void *prev = nullptr;
	while (map->map_get_next_key(prev, &next) == 0) {
		REQUIRE(seen.insert(next).second);
		const auto *v =
			static_cast<const uint64_t *>(map->elem_lookup(&next));
		REQUIRE(v != nullptr);
		REQUIRE(*v == ((next & 0xffff) == 0 ? (uint64_t)next * 7 : next));
		key = next;
		prev = &key;
	}
	REQUIRE(seen.size() == max_entries);
	for (uint32_t p = 0; p < procs; p++)
		for (uint32_t i = 0; i < keys_per_proc; i += 3)
			REQUIRE(seen.count(clustered_key(i * procs + p)) == 1);
}

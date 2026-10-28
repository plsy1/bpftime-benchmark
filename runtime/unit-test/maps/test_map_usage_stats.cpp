// Opt-in map usage counters (BPFTIME_ENABLE_MAP_USAGE_STATS): every program
// and syscall entry point of a map must be counted once and classified.
#include "bpf_map/map_usage_stats.hpp"
#include "handler/map_handler.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <linux/bpf.h>
#include <sstream>
#include <string>
#include <chrono>
#include <future>
#include <thread>
#include <unistd.h>

using namespace boost::interprocess;
using namespace bpftime;

TEST_CASE("Map usage counters classify program and syscall operations",
	  "[map_usage]")
{
#if !BPFTIME_ENABLE_MAP_USAGE_STATS
	SKIP("built without BPFTIME_ENABLE_MAP_USAGE_STATS");
#else
	const char *SHM_NAME = "BPFTIME_TEST_MAP_USAGE_STATS";
	const std::string path =
		"/tmp/bpftime-map-usage-test-" + std::to_string(getpid());
	std::remove(path.c_str());
	REQUIRE(setenv("BPFTIME_MAP_USAGE_STATS_FILE", path.c_str(), 1) == 0);
	map_usage::reset();
	shared_memory_object::remove(SHM_NAME);
	managed_shared_memory segment(create_only, SHM_NAME, 4 << 20);
	bpf_map_handler hash(1, BPF_MAP_TYPE_HASH, 4, 8, 64, 0, "usage_hash",
			     segment);
	REQUIRE(hash.map_init(segment) == 0);
	bpf_map_handler array(2, BPF_MAP_TYPE_ARRAY, 4, 8, 4, 0,
			      "usage_array", segment);
	REQUIRE(array.map_init(segment) == 0);

	uint32_t k1 = 1, k2 = 2, out_of_range = 9;
	uint64_t v = 7;
	// Program side, typed entry points.
	REQUIRE(hash.map_update_hash(&k1, &v, BPF_ANY) == 0); // insert
	REQUIRE(hash.map_update_hash(&k1, &v, BPF_ANY) == 0); // existing
	REQUIRE(hash.map_update_hash(&k1, &v, BPF_NOEXIST) < 0); // failed
	REQUIRE(hash.map_lookup_hash(&k1) != nullptr);
	REQUIRE(hash.map_lookup_hash(&k2) == nullptr);
	// Program side, generic entry points.
	REQUIRE(hash.map_update_elem(&k2, &v, BPF_ANY, false) == 0); // insert
	REQUIRE(hash.map_delete_elem(&k2, false) == 0);
	REQUIRE(hash.map_delete_hash(&k2) < 0);
	// Another thread.
	std::thread([&] { REQUIRE(hash.map_lookup_hash(&k1) != nullptr); })
		.join();
	// The loader through bpf().
	REQUIRE(hash.map_lookup_elem(&k1, true) != nullptr);
	REQUIRE(hash.map_update_elem(&k1, &v, BPF_ANY, true) == 0);
	// Arrays.
	REQUIRE(array.map_lookup_array(&k1) != nullptr);
	REQUIRE(array.map_lookup_array(&out_of_range) == nullptr);
	REQUIRE(array.map_update_array(&k1, &v, BPF_ANY) == 0);

	const map_usage::map_record *h = nullptr, *a = nullptr;
	auto records = map_usage::snapshot();
	for (const auto &r : records) {
		if (r.name == "usage_hash")
			h = &r;
		if (r.name == "usage_array")
			a = &r;
	}
	REQUIRE(h != nullptr);
	REQUIRE(a != nullptr);
	using map_usage::op;
	const auto &hp = h->by_side[(int)map_usage::side::program].ops;
	CHECK(hp[(int)op::update_insert] == 2);
	CHECK(hp[(int)op::update_existing] == 1);
	CHECK(hp[(int)op::update_failed] == 1);
	CHECK(hp[(int)op::lookup_hit] == 2);
	CHECK(hp[(int)op::lookup_miss] == 1);
	CHECK(hp[(int)op::delete_hit] == 1);
	CHECK(hp[(int)op::delete_miss] == 1);
	CHECK(h->by_side[(int)map_usage::side::program].threads == 2);
	const auto &hs = h->by_side[(int)map_usage::side::syscall];
	CHECK(hs.ops[(int)op::lookup_hit] == 1);
	CHECK(hs.ops[(int)op::update_existing] == 1);
	CHECK(hs.threads == 1);
	CHECK(h->key_size == 4);
	CHECK(h->value_size == 8);
	CHECK(h->max_entries == 64);
	const auto &ap = a->by_side[(int)map_usage::side::program].ops;
	CHECK(ap[(int)op::lookup_hit] == 1);
	CHECK(ap[(int)op::lookup_miss] == 1);
	CHECK(ap[(int)op::update_existing] == 1);

	REQUIRE(map_usage::dump(path.c_str()));
	std::ifstream file(path);
	std::stringstream text;
	text << file.rdbuf();
	CHECK(text.str().find("\"map\":\"usage_hash\"") != std::string::npos);
	CHECK(text.str().find("\"update_insert\":2") != std::string::npos);

	hash.map_free(segment);
	array.map_free(segment);
	unsetenv("BPFTIME_MAP_USAGE_STATS_FILE");
	map_usage::reset();
	std::remove(path.c_str());
	shared_memory_object::remove(SHM_NAME);
#endif
}

#if BPFTIME_ENABLE_MAP_USAGE_STATS
namespace
{
const bpf_map_handler *nested_map;
void nested_lookup()
{
	uint32_t key = 1;
	nested_map->map_lookup_array(&key);
}
} // namespace
#endif

// Counting allocates; under memleak or a malloc probe that allocation runs a
// program whose map operation re-enters the counters on the same thread. It
// used to take the held table lock again and hang the traced process.
TEST_CASE("Map usage counters do not deadlock on nested operations",
	  "[map_usage]")
{
#if !BPFTIME_ENABLE_MAP_USAGE_STATS
	SKIP("built without BPFTIME_ENABLE_MAP_USAGE_STATS");
#else
	const char *SHM_NAME = "BPFTIME_TEST_MAP_USAGE_NESTED";
	REQUIRE(setenv("BPFTIME_MAP_USAGE_STATS_FILE", "/dev/null", 1) == 0);
	map_usage::reset();
	shared_memory_object::remove(SHM_NAME);
	managed_shared_memory segment(create_only, SHM_NAME, 1 << 20);
	bpf_map_handler array(1, BPF_MAP_TYPE_ARRAY, 4, 8, 4, 0,
			      "nested_array", segment);
	REQUIRE(array.map_init(segment) == 0);
	nested_map = &array;
	map_usage::nested_call_hook_for_test = nested_lookup;
	auto outer = std::async(std::launch::async, [&] {
		uint32_t key = 0;
		return array.map_lookup_array(&key) != nullptr;
	});
	const bool finished =
		outer.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
	map_usage::nested_call_hook_for_test = nullptr;
	if (!finished) {
		// The worker is stuck on the lock; leave it behind, and keep the
		// exit dump from waiting for that lock too.
		new std::future<bool>(std::move(outer));
		unsetenv("BPFTIME_MAP_USAGE_STATS_FILE");
		FAIL("a nested map operation deadlocked the counters");
	}
	REQUIRE(outer.get());
	auto records = map_usage::snapshot();
	REQUIRE(records.size() == 1);
	// The outer lookup is counted, the nested one (the counters' own
	// allocation in real runs) is not.
	CHECK(records[0].by_side[(int)map_usage::side::program]
		      .ops[(int)map_usage::op::lookup_hit] == 1);
	array.map_free(segment);
	unsetenv("BPFTIME_MAP_USAGE_STATS_FILE");
	map_usage::reset();
	shared_memory_object::remove(SHM_NAME);
#endif
}

// The exit dump copies the counts under the table lock; under memleak or a
// malloc probe that copy re-entered the counters and hung the traced process
// at exit.
TEST_CASE("Map usage dump does not deadlock on nested operations",
	  "[map_usage]")
{
#if !BPFTIME_ENABLE_MAP_USAGE_STATS
	SKIP("built without BPFTIME_ENABLE_MAP_USAGE_STATS");
#else
	const char *SHM_NAME = "BPFTIME_TEST_MAP_USAGE_DUMP";
	REQUIRE(setenv("BPFTIME_MAP_USAGE_STATS_FILE", "/dev/null", 1) == 0);
	map_usage::reset();
	shared_memory_object::remove(SHM_NAME);
	managed_shared_memory segment(create_only, SHM_NAME, 1 << 20);
	bpf_map_handler array(1, BPF_MAP_TYPE_ARRAY, 4, 8, 4, 0, "dump_array",
			      segment);
	REQUIRE(array.map_init(segment) == 0);
	uint32_t key = 0;
	REQUIRE(array.map_lookup_array(&key) != nullptr);
	nested_map = &array;
	map_usage::nested_call_hook_for_test = nested_lookup;
	auto dumped = std::async(std::launch::async,
				 [] { return map_usage::dump("/dev/null"); });
	const bool finished =
		dumped.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
	map_usage::nested_call_hook_for_test = nullptr;
	if (!finished) {
		new std::future<bool>(std::move(dumped));
		unsetenv("BPFTIME_MAP_USAGE_STATS_FILE");
		FAIL("the dump deadlocked on a nested map operation");
	}
	REQUIRE(dumped.get());
	auto records = map_usage::snapshot();
	REQUIRE(records.size() == 1);
	CHECK(records[0].by_side[(int)map_usage::side::program]
		      .ops[(int)map_usage::op::lookup_hit] == 1);
	array.map_free(segment);
	unsetenv("BPFTIME_MAP_USAGE_STATS_FILE");
	map_usage::reset();
	shared_memory_object::remove(SHM_NAME);
#endif
}

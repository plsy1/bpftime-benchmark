/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#ifndef _BPFTIME_MAP_USAGE_STATS_HPP
#define _BPFTIME_MAP_USAGE_STATS_HPP

// Opt-in runtime map usage counters (BPFTIME_ENABLE_MAP_USAGE_STATS), for
// surveying how real tools use their maps: operations per map, lookups that
// miss, inserts versus updates of existing keys, deletes that find nothing,
// and how many threads touch each map. Counting needs the environment
// variable BPFTIME_MAP_USAGE_STATS_FILE as well; each process appends one
// JSON line per map to that file at exit. Off by default: the hooks compile
// to nothing.

#if BPFTIME_ENABLE_MAP_USAGE_STATS
#include <cstdint>
#include <string>
#include <vector>

namespace bpftime
{
class bpf_map_handler;
namespace map_usage
{
enum class op : int {
	lookup_hit,
	lookup_miss,
	update_insert,
	update_existing,
	update_failed,
	delete_hit,
	delete_miss,
	count
};

// Who made the call: a program (helper or JIT code) or the loader through
// bpf() (the syscall server).
enum class side : int { program, syscall, count };

struct counters {
	uint64_t ops[(int)op::count] = {};
	uint64_t threads = 0;
	// Monotonic nanoseconds of the first and last counted operation.
	uint64_t first_ns = 0;
	uint64_t last_ns = 0;
};

struct map_record {
	std::string name;
	int type = 0;
	uint32_t key_size = 0;
	uint32_t value_size = 0;
	uint32_t max_entries = 0;
	counters by_side[(int)side::count];
};

bool enabled();
void record(const bpf_map_handler &map, op operation, bool from_syscall);
void record_lookup(const bpf_map_handler &map, const void *result,
		   bool from_syscall);
// existed: whether the key was present before the update.
void record_update(const bpf_map_handler &map, long result, bool existed,
		   bool from_syscall);
void record_delete(const bpf_map_handler &map, long result, bool from_syscall);

// For tests and the exit dump.
std::vector<map_record> snapshot();
void reset();
// Appends one JSON line per counted map to path; returns false on error.
bool dump(const char *path);
// Tests only: called under the table lock (while counting and in snapshots,
// which the exit dump takes), where a probed allocation would re-enter the
// counters.
extern void (*nested_call_hook_for_test)();
} // namespace map_usage
} // namespace bpftime
#endif

#endif

/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#include "bpf_map/map_usage_stats.hpp"

#if BPFTIME_ENABLE_MAP_USAGE_STATS
#include "handler/map_handler.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <mutex>
#include <new>
#include <pthread.h>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>

namespace bpftime::map_usage
{
namespace
{
struct entry {
	map_record record;
	std::unordered_set<pid_t> tids[(int)side::count];
};

// -1: not decided yet, 0: off, 1: on (BPFTIME_MAP_USAGE_STATS_FILE is set).
std::atomic<int> state{ -1 };
std::mutex table_lock;
std::unordered_map<const void *, entry> *table;
std::once_flag exit_hooks_once;
// Everything that holds the table lock allocates (the first operation of a
// map or thread, snapshot copies, the exit dump), and an allocation can hit a
// probe (malloc, memleak) whose program operates on a map and so re-enters
// here on the same thread with the lock held. While a thread is inside the
// counters its map operations are not counted: they come from the agent's own
// allocations, not from the traced program.
thread_local bool inside_counters = false;

struct counters_scope {
	const bool nested = inside_counters;
	counters_scope()
	{
		inside_counters = true;
	}
	~counters_scope()
	{
		inside_counters = nested;
	}
};

uint64_t now_ns()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

void dump_at_exit()
{
	if (const char *path = getenv("BPFTIME_MAP_USAGE_STATS_FILE"))
		dump(path);
}

// A fork child inherits the parent's counts; it reports its own only. The
// lock may have been held by another parent thread at the fork.
void forget_parent_counts()
{
	new (&table_lock) std::mutex();
	if (table)
		table->clear();
}

void install_exit_hooks()
{
	std::call_once(exit_hooks_once, [] {
		atexit(dump_at_exit);
		pthread_atfork(nullptr, nullptr, forget_parent_counts);
	});
}

std::string json_escape(const std::string &text)
{
	std::string out;
	for (char c : text) {
		if (c == '"' || c == '\\')
			out.push_back('\\');
		if ((unsigned char)c < 0x20)
			continue;
		out.push_back(c);
	}
	return out;
}

std::string process_name()
{
	char name[64] = {};
	int fd = open("/proc/self/comm", O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return "";
	ssize_t len = read(fd, name, sizeof(name) - 1);
	close(fd);
	if (len <= 0)
		return "";
	std::string out(name, (size_t)len);
	while (!out.empty() && out.back() == '\n')
		out.pop_back();
	return out;
}

const char *op_names[(int)op::count] = { "lookup_hit",	   "lookup_miss",
					 "update_insert",  "update_existing",
					 "update_failed",  "delete_hit",
					 "delete_miss" };
} // namespace

void (*nested_call_hook_for_test)() = nullptr;

bool enabled()
{
	int current = state.load(std::memory_order_relaxed);
	if (current < 0) {
		current = getenv("BPFTIME_MAP_USAGE_STATS_FILE") != nullptr;
		state.store(current, std::memory_order_relaxed);
	}
	return current == 1;
}

void record(const bpf_map_handler &map, op operation, bool from_syscall)
{
	if (!enabled() || inside_counters)
		return;
	counters_scope scope;
	install_exit_hooks();
	const pid_t tid = gettid();
	const uint64_t ns = now_ns();
	const int who = (int)(from_syscall ? side::syscall : side::program);
	std::lock_guard<std::mutex> guard(table_lock);
	if (!table)
		table = new std::unordered_map<const void *, entry>();
	auto [it, inserted] = table->try_emplace(&map);
	entry &e = it->second;
	if (inserted) {
		// Copied now: the handler lives in the shared segment, which may
		// already be unmapped when the exit dump runs.
		e.record.name = std::string(map.name.c_str());
		e.record.type = (int)map.type;
		e.record.key_size = map.get_key_size();
		e.record.value_size = map.get_value_size();
		e.record.max_entries = map.get_max_entries();
	}
	counters &c = e.record.by_side[who];
	c.ops[(int)operation]++;
	if (e.tids[who].insert(tid).second)
		c.threads++;
	if (!c.first_ns)
		c.first_ns = ns;
	c.last_ns = ns;
	if (nested_call_hook_for_test)
		nested_call_hook_for_test();
}

void record_lookup(const bpf_map_handler &map, const void *result,
		   bool from_syscall)
{
	record(map, result ? op::lookup_hit : op::lookup_miss, from_syscall);
}

void record_update(const bpf_map_handler &map, long result, bool existed,
		   bool from_syscall)
{
	record(map,
	       result < 0 ? op::update_failed :
	       existed	  ? op::update_existing :
			    op::update_insert,
	       from_syscall);
}

void record_delete(const bpf_map_handler &map, long result, bool from_syscall)
{
	record(map, result < 0 ? op::delete_miss : op::delete_hit,
	       from_syscall);
}

std::vector<map_record> snapshot()
{
	counters_scope scope;
	std::lock_guard<std::mutex> guard(table_lock);
	if (nested_call_hook_for_test)
		nested_call_hook_for_test();
	std::vector<map_record> out;
	if (table)
		for (const auto &[key, e] : *table)
			out.push_back(e.record);
	return out;
}

void reset()
{
	counters_scope scope;
	std::lock_guard<std::mutex> guard(table_lock);
	if (table)
		table->clear();
	state.store(-1, std::memory_order_relaxed);
}

bool dump(const char *path)
{
	counters_scope scope;
	const auto records = snapshot();
	if (records.empty())
		return true;
	int fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
	if (fd < 0)
		return false;
	const std::string comm = json_escape(process_name());
	bool ok = true;
	for (const auto &r : records) {
		std::string line = "{\"pid\":" + std::to_string(getpid()) +
				   ",\"comm\":\"" + comm + "\",\"map\":\"" +
				   json_escape(r.name) +
				   "\",\"type\":" + std::to_string(r.type) +
				   ",\"key_size\":" + std::to_string(r.key_size) +
				   ",\"value_size\":" +
				   std::to_string(r.value_size) +
				   ",\"max_entries\":" +
				   std::to_string(r.max_entries);
		const char *sides[] = { "program", "syscall" };
		for (int s = 0; s < (int)side::count; s++) {
			const counters &c = r.by_side[s];
			line += std::string(",\"") + sides[s] + "\":{";
			for (int o = 0; o < (int)op::count; o++)
				line += std::string("\"") + op_names[o] +
					"\":" + std::to_string(c.ops[o]) + ",";
			line += "\"threads\":" + std::to_string(c.threads) +
				",\"first_ns\":" + std::to_string(c.first_ns) +
				",\"last_ns\":" + std::to_string(c.last_ns) +
				"}";
		}
		line += "}\n";
		// One write per line: O_APPEND keeps concurrent writers apart.
		if (write(fd, line.data(), line.size()) != (ssize_t)line.size())
			ok = false;
	}
	close(fd);
	return ok;
}
} // namespace bpftime::map_usage
#endif

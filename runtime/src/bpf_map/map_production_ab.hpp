// SPDX-License-Identifier: MIT
// Diagnostic-only runtime switches for production-agent map path A/B tests.
#pragma once

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <string_view>

namespace bpftime
{
enum class map_production_ab_mode {
	base,
	cache_control,
	cached_handler,
	direct_map,
	percpu_array_direct,
	percpu_array_fixed_cpu,
	percpu_array_no_copy,
	percpu_hash_lookup_no_find,
	percpu_hash_lookup_raw_key_copy,
	percpu_hash_lookup_cached_hash,
	percpu_hash_lookup_fixed4_equal,
	percpu_hash_lookup_cached_hash_fixed4_equal,
	percpu_hash_update_no_find,
	percpu_hash_update_no_copy,
	percpu_hash_delete_defer_reclaim,
};

inline map_production_ab_mode get_map_production_ab_mode()
{
	static const map_production_ab_mode mode = [] {
		const char *raw = std::getenv("BPFTIME_MAP_PRODUCTION_AB");
		const std::string_view value = raw ? raw : "base";
		if (value == "cache_control")
			return map_production_ab_mode::cache_control;
		if (value == "cached_handler")
			return map_production_ab_mode::cached_handler;
		if (value == "direct_map")
			return map_production_ab_mode::direct_map;
		if (value == "percpu_array_direct")
			return map_production_ab_mode::percpu_array_direct;
		if (value == "percpu_array_fixed_cpu")
			return map_production_ab_mode::percpu_array_fixed_cpu;
		if (value == "percpu_array_no_copy")
			return map_production_ab_mode::percpu_array_no_copy;
		if (value == "percpu_hash_lookup_no_find")
			return map_production_ab_mode::percpu_hash_lookup_no_find;
		if (value == "percpu_hash_lookup_raw_key_copy")
			return map_production_ab_mode::percpu_hash_lookup_raw_key_copy;
		if (value == "percpu_hash_lookup_cached_hash")
			return map_production_ab_mode::percpu_hash_lookup_cached_hash;
		if (value == "percpu_hash_lookup_fixed4_equal")
			return map_production_ab_mode::percpu_hash_lookup_fixed4_equal;
		if (value == "percpu_hash_lookup_cached_hash_fixed4_equal")
			return map_production_ab_mode::percpu_hash_lookup_cached_hash_fixed4_equal;
		if (value == "percpu_hash_update_no_find")
			return map_production_ab_mode::percpu_hash_update_no_find;
		if (value == "percpu_hash_update_no_copy")
			return map_production_ab_mode::percpu_hash_update_no_copy;
		if (value == "percpu_hash_delete_defer_reclaim")
			return map_production_ab_mode::percpu_hash_delete_defer_reclaim;
		return map_production_ab_mode::base;
	}();
	return mode;
}

inline bool map_production_ab_cached_hash()
{
	const auto mode = get_map_production_ab_mode();
	return mode == map_production_ab_mode::percpu_hash_lookup_cached_hash ||
	       mode == map_production_ab_mode::percpu_hash_lookup_cached_hash_fixed4_equal;
}

inline bool map_production_ab_fixed4_equal()
{
	const auto mode = get_map_production_ab_mode();
	return mode == map_production_ab_mode::percpu_hash_lookup_fixed4_equal ||
	       mode == map_production_ab_mode::percpu_hash_lookup_cached_hash_fixed4_equal;
}

inline bool map_production_ab_stats_enabled()
{
	static const bool enabled = [] {
		const char *raw = std::getenv("BPFTIME_MAP_PRODUCTION_AB_STATS");
		return raw && std::string_view(raw) == "1";
	}();
	return enabled;
}

struct map_production_ab_lookup_stats {
	uint64_t lookups = 0;
	uint64_t hits = 0;
	uint64_t misses = 0;
	uint64_t hash_calls = 0;
	uint64_t equal_calls = 0;
	~map_production_ab_lookup_stats()
	{
		if (!map_production_ab_stats_enabled())
			return;
		const char *mode = std::getenv("BPFTIME_MAP_PRODUCTION_AB");
		std::fprintf(stderr,
			     "BPFTIME_MAP_PRODUCTION_AB_STATS mode=%s lookups=%llu hits=%llu misses=%llu hash_calls=%llu equal_calls=%llu\n",
			     mode ? mode : "base",
			     (unsigned long long)lookups,
			     (unsigned long long)hits,
			     (unsigned long long)misses,
			     (unsigned long long)hash_calls,
			     (unsigned long long)equal_calls);
	}
};

inline map_production_ab_lookup_stats &map_production_ab_stats()
{
	static thread_local map_production_ab_lookup_stats stats;
	return stats;
}

inline bool &map_production_ab_lookup_active()
{
	static thread_local bool active = false;
	return active;
}
} // namespace bpftime

// SPDX-License-Identifier: MIT
// Diagnostic-only runtime switches for production-agent map path A/B tests.
#pragma once

#include <cstdlib>
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
} // namespace bpftime

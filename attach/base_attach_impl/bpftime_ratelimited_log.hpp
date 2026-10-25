/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026, eunomia-bpf org
 * All rights reserved.
 */
#ifndef _BPFTIME_RATELIMITED_LOG_HPP
#define _BPFTIME_RATELIMITED_LOG_HPP
#include "spdlog/spdlog.h"
#include <atomic>
#include <cstdint>

// Errors on paths that run per event (helpers, probe dispatch, map access
// from programs) must not log on every invocation: a broken probe at 1M calls
// per second turned into 1M log lines per second. Each call site logs its
// 1st, 10th, 100th, ... occurrence with the count. fmt must be a literal.
namespace bpftime::detail
{
inline bool ratelimit_should_log(uint64_t n)
{
	while (n % 10 == 0)
		n /= 10;
	return n == 1;
}
} // namespace bpftime::detail

#define BPFTIME_LOG_RATELIMITED_(level_macro, fmt, ...)                        \
	do {                                                                   \
		static std::atomic<uint64_t> bpftime_ratelimit_count_{ 0 };   \
		const uint64_t bpftime_ratelimit_n_ =                          \
			bpftime_ratelimit_count_.fetch_add(                    \
				1, std::memory_order_relaxed) +                \
			1;                                                     \
		if (::bpftime::detail::ratelimit_should_log(                   \
			    bpftime_ratelimit_n_))                             \
			level_macro(fmt " (occurrence {})", ##__VA_ARGS__,     \
				    bpftime_ratelimit_n_);                     \
	} while (0)

#define BPFTIME_LOG_ERROR_RATELIMITED(fmt, ...)                                \
	BPFTIME_LOG_RATELIMITED_(SPDLOG_ERROR, fmt, ##__VA_ARGS__)
#define BPFTIME_LOG_WARN_RATELIMITED(fmt, ...)                                 \
	BPFTIME_LOG_RATELIMITED_(SPDLOG_WARN, fmt, ##__VA_ARGS__)

#endif

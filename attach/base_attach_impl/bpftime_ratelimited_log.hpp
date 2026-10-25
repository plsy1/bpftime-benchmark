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

// Out of line and cold, with the arguments passed by value: an inlined
// logging path made the hot caller take addresses of its locals, so every
// call paid a stack-protector canary and spills (bpf_probe_read +31% in the
// full ten-round suite).
template <typename... Args>
__attribute__((cold, noinline)) void
log_ratelimited(std::atomic<uint64_t> &count, spdlog::level::level_enum level,
		const char *file, int line, const char *func,
		spdlog::format_string_t<Args..., uint64_t> fmt, Args... args)
{
	const uint64_t n = count.fetch_add(1, std::memory_order_relaxed) + 1;
	if (!ratelimit_should_log(n))
		return;
	if (auto *logger = spdlog::default_logger_raw())
		// fmt was checked at the call site; do not re-check here.
		logger->log(spdlog::source_loc{ file, line, func }, level,
			    spdlog::fmt_lib::runtime(fmt.get()), args..., n);
}
} // namespace bpftime::detail

#define BPFTIME_LOG_RATELIMITED_(level, fmt, ...)                              \
	do {                                                                   \
		static std::atomic<uint64_t> bpftime_ratelimit_count_{ 0 };   \
		::bpftime::detail::log_ratelimited(                            \
			bpftime_ratelimit_count_, level, __FILE__, __LINE__,   \
			static_cast<const char *>(__FUNCTION__),               \
			fmt " (occurrence {})", ##__VA_ARGS__);                \
	} while (0)

#define BPFTIME_LOG_ERROR_RATELIMITED(fmt, ...)                                \
	BPFTIME_LOG_RATELIMITED_(spdlog::level::err, fmt, ##__VA_ARGS__)
#define BPFTIME_LOG_WARN_RATELIMITED(fmt, ...)                                 \
	BPFTIME_LOG_RATELIMITED_(spdlog::level::warn, fmt, ##__VA_ARGS__)

#endif

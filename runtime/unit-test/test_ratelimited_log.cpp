#include "bpftime_ratelimited_log.hpp"
#include <catch2/catch_test_macros.hpp>
#include <spdlog/sinks/ostream_sink.h>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>

extern "C" int64_t bpftime_probe_read(uint64_t dst, int64_t size, uint64_t ptr,
				      uint64_t, uint64_t);

TEST_CASE("Rate-limited logging picks 1, 10, 100, ...", "[ratelimit]")
{
	using bpftime::detail::ratelimit_should_log;
	for (uint64_t n : { 1, 10, 100, 1000, 1000000 })
		REQUIRE(ratelimit_should_log(n));
	for (uint64_t n : { 2, 9, 11, 20, 99, 101, 110, 999, 1001 })
		REQUIRE_FALSE(ratelimit_should_log(n));
}

// A helper failing on every invocation used to log every time (a broken
// probe at 1M calls/s wrote 1M lines/s).
TEST_CASE("A helper error on every call logs at 1, 10, 100, 1000",
	  "[ratelimit]")
{
	std::ostringstream out;
	auto previous = spdlog::default_logger();
	auto logger = std::make_shared<spdlog::logger>(
		"ratelimit_test",
		std::make_shared<spdlog::sinks::ostream_sink_mt>(out));
	logger->set_level(spdlog::level::trace);
	spdlog::set_default_logger(logger);
	uint64_t buf = 0;
	for (int i = 0; i < 1000; i++)
		REQUIRE(bpftime_probe_read((uint64_t)&buf, -1, (uint64_t)&buf, 0,
					   0) == -EFAULT);
	spdlog::set_default_logger(previous);
	const std::string text = out.str();
	size_t lines = 0;
	for (char c : text)
		lines += c == '\n';
	REQUIRE(lines == 4);
	REQUIRE(text.find("Invalid size: -1 (occurrence 1)") != std::string::npos);
	REQUIRE(text.find("(occurrence 1000)") != std::string::npos);
}

#include "catch2/catch_test_macros.hpp"
#include "frida-gum.h"
#include "frida_register_def.hpp"
#include "frida_uprobe_attach_impl.hpp"
#include "spdlog/spdlog.h"
#include <array>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
using namespace bpftime;
using namespace attach;

extern "C" uint64_t __bpftime_test_stripped_back_trace__func1(uint64_t x);
extern "C" uint64_t __bpftime_test_stripped_back_trace__func5(uint64_t x);

extern "C" __attribute__((__noinline__)) uint64_t
__bpftime_test_attach_with_back_trace__func5(uint64_t x)
{
	asm("");
	uint64_t result = 0;
	for (int i = 1; i <= x; i++) {
		result += (uint64_t)sqrt(i);
	}
	return result;
}

extern "C" __attribute__((__noinline__)) uint64_t
__bpftime_test_attach_with_back_trace__func4(uint64_t x)
{
	asm("");
	return __bpftime_test_attach_with_back_trace__func5(x) + 1;
}

extern "C" __attribute__((__noinline__)) uint64_t
__bpftime_test_attach_with_back_trace__func3(uint64_t x)
{
	asm("");
	return __bpftime_test_attach_with_back_trace__func4(x) + 2;
}

extern "C" __attribute__((__noinline__)) uint64_t
__bpftime_test_attach_with_back_trace__func2(uint64_t x)
{
	asm("");
	return __bpftime_test_attach_with_back_trace__func3(x) + 3;
}

extern "C" __attribute__((__noinline__)) uint64_t
__bpftime_test_attach_with_back_trace__func1(uint64_t x)
{
	asm("");
	return __bpftime_test_attach_with_back_trace__func2(x) + 4;
}

TEST_CASE("Test with backtrace")
{
	bool use_fuzzy_backtracer = false;
	SECTION("accurate")
	{
		use_fuzzy_backtracer = false;
	}
	SECTION("fuzzy")
	{
		use_fuzzy_backtracer = true;
	}
	frida_attach_impl impl(use_fuzzy_backtracer);
	bool invoked = false;
	impl.create_uprobe_at(
		(void *)&__bpftime_test_attach_with_back_trace__func5,
		[&](const pt_regs &regs) {
			invoked = true;
			auto stack = (std::vector<uint64_t> *)
					     impl.call_attach_specific_function(
						     "generate_stack", nullptr);
			REQUIRE_FALSE(stack->empty());
			if (!use_fuzzy_backtracer) {
				for (int i = 0; i < 4; i++) {
					auto addr = stack->at(i);
					GumDebugSymbolDetails debug_details;
					REQUIRE(gum_symbol_details_from_address(
							(GumReturnAddress)addr,
							&debug_details) == true);
					SPDLOG_INFO("symbol name {}",
						    debug_details.symbol_name);
					auto expected_name = std::string(
						"__bpftime_test_attach_with_back_trace__func");
					REQUIRE(std::string(debug_details.symbol_name)
							.starts_with(expected_name));
				}
			}

			delete stack;
		});
	REQUIRE(__bpftime_test_attach_with_back_trace__func1(100) == 635);
	REQUIRE(invoked == true);
}

TEST_CASE("Uretprobe callbacks can generate a backtrace")
{
	frida_attach_impl impl(false);
	auto generate = [&]() {
		auto stack = (std::vector<uint64_t> *)
				     impl.call_attach_specific_function(
					     "generate_stack", nullptr);
		REQUIRE(stack != nullptr);
		std::vector<uint64_t> copy = *stack;
		delete stack;
		return copy;
	};
	auto symbol = [](uint64_t addr) {
		GumDebugSymbolDetails details;
		REQUIRE(gum_symbol_details_from_address((GumReturnAddress)addr,
							&details));
		SPDLOG_INFO("symbol name {}", details.symbol_name);
		return std::string(details.symbol_name);
	};
	std::vector<uint64_t> at_entry, at_return;
	impl.create_uprobe_at((void *)&__bpftime_test_attach_with_back_trace__func5,
			      [&](const pt_regs &) { at_entry = generate(); });
	impl.create_uretprobe_at(
		(void *)&__bpftime_test_attach_with_back_trace__func5,
		[&](const pt_regs &) { at_return = generate(); });
	REQUIRE(__bpftime_test_attach_with_back_trace__func1(100) == 635);

	const std::string prefix = "__bpftime_test_attach_with_back_trace__func";
	REQUIRE(at_entry.size() >= 3);
	REQUIRE(at_return.size() >= 3);
#if defined(__aarch64__)
	// Kernel semantics (arm64 perf_callchain_user): the pc first, then the
	// frame-record chain. At entry the pc is the probed function and the
	// caller (func4) has no record yet; at return the pc is the return
	// address in func4. Above that both see the same chain.
	REQUIRE(at_entry[0] ==
		(uint64_t)(uintptr_t)&__bpftime_test_attach_with_back_trace__func5);
	REQUIRE(symbol(at_entry[1]).starts_with(prefix + "3"));
	REQUIRE(symbol(at_return[0]).starts_with(prefix + "4"));
	REQUIRE(std::vector<uint64_t>(at_entry.begin() + 1, at_entry.end()) ==
		std::vector<uint64_t>(at_return.begin() + 1, at_return.end()));
#else
	// Gum backtracer: callers seen at entry are the stack at return.
	REQUIRE(at_return == at_entry);
	REQUIRE(symbol(at_return[0]).starts_with(prefix + "4"));
	REQUIRE(symbol(at_return[1]).starts_with(prefix + "3"));
#endif
	// The context is cleared after the callbacks.
	REQUIRE(impl.call_attach_specific_function("generate_stack", nullptr) ==
		nullptr);
}

TEST_CASE("Fuzzy backtracer recovers a call chain without unwind metadata")
{
	std::array<std::size_t, 2> fixture_frames{};
	for (const bool use_fuzzy_backtracer : { false, true }) {
		frida_attach_impl impl(use_fuzzy_backtracer);
		impl.create_uprobe_at(
			(void *)&__bpftime_test_stripped_back_trace__func5,
			[&](const pt_regs &) {
				auto stack = (std::vector<uint64_t> *)
						     impl.call_attach_specific_function(
							     "generate_stack", nullptr);
				REQUIRE(stack != nullptr);
				for (const auto addr : *stack) {
					GumDebugSymbolDetails details;
					if (gum_symbol_details_from_address(
						    (GumReturnAddress)addr,
						    &details) &&
					    std::string(details.symbol_name)
						    .starts_with(
							    "__bpftime_test_stripped_back_trace__func")) {
						fixture_frames[use_fuzzy_backtracer]++;
					}
				}
				delete stack;
			});
		REQUIRE(__bpftime_test_stripped_back_trace__func1(1) == 16);
	}

	REQUIRE(fixture_frames[1] >= 3);
	REQUIRE(fixture_frames[1] > fixture_frames[0]);
}

#include "bpftime_config.hpp"
#include "bpftime_helper_group.hpp"
#include "bpftime_prog.hpp"
#include "bpftime_ufunc.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>

using namespace bpftime;

// The path helpers (benchmark/fuse's fs-cache records directory fds with
// them) sat under a macro nothing defined: the ufunc group lacked them and a
// program calling one failed to JIT ("Ext func not found") and was disabled.
TEST_CASE("The ufunc group provides the path helpers", "[helpers]")
{
	struct {
		char first[16];
		char second[16];
		char out[256];
	} ctx = { "a.txt", "b", {} };
	auto run = [&](int32_t helper, bool two_paths) {
		std::memset(ctx.out, 0, sizeof(ctx.out));
		const uint8_t out_reg = two_paths ? 3 : 2;
		const uint8_t size_reg = two_paths ? 4 : 3;
		const ebpf_inst code[] = {
			// r6 = ctx; r1 = ctx (first path)
			{ .code = 0xbf, .dst_reg = 6, .src_reg = 1 },
			// r2 = ctx + 16 (second path)
			{ .code = 0xbf, .dst_reg = 2, .src_reg = 6 },
			{ .code = 0x07, .dst_reg = 2, .imm = 16 },
			// output buffer and its size
			{ .code = 0xbf, .dst_reg = out_reg, .src_reg = 6 },
			{ .code = 0x07, .dst_reg = out_reg, .imm = 32 },
			{ .code = 0xb7, .dst_reg = size_reg, .imm = 255 },
			{ .code = 0x85, .imm = helper },
			{ .code = 0xb7, .dst_reg = 0, .imm = 0 },
			{ .code = 0x95 },
		};
		bpftime_prog prog(code, std::size(code), "path_helper",
				  runtime_config{});
		REQUIRE(bpftime_helper_group::get_ufunc_helper_group()
				.add_helper_group_to_prog(&prog) == 0);
		REQUIRE(prog.bpftime_prog_load(true) == 0);
		uint64_t ret = 1;
		REQUIRE(prog.bpftime_prog_exec(&ctx, sizeof(ctx), &ret) == 0);
		REQUIRE(ret == 0);
		return std::string(ctx.out);
	};
	REQUIRE(run(EXTENDED_HELPER_GET_ABS_PATH_ID, false) ==
		(std::filesystem::current_path() / "a.txt").string());
	REQUIRE(run(EXTENDED_HELPER_PATH_JOIN_ID, true) == "a.txt/b");
}

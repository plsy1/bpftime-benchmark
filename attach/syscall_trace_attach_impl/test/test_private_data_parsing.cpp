#include "syscall_table.hpp"
#include "syscall_trace_attach_private_data.hpp"
#include <catch2/catch_test_macros.hpp>
#include <syscall_trace_attach_impl.hpp>
#include <string>
#include <sys/syscall.h>
#include <utility>
using namespace bpftime::attach;

TEST_CASE("Test resolving private data")
{
	auto &tp_table = get_global_syscall_tracepoint_name_table();

	syscall_trace_attach_private_data data;
	bool set = false;
	SECTION("global enter")
	{
		for (auto &[k, v] : tp_table) {
			if (v == GLOBAL_SYS_ENTER_NAME) {
				REQUIRE(data.initialize_from_string(
						std::to_string(k)) == 0);
				set = true;
				break;
			}
		}
		REQUIRE(set);
		REQUIRE(data.is_enter == true);
		REQUIRE(data.sys_nr == -1);
	}
	SECTION("global exit")
	{
		for (auto &[k, v] : tp_table) {
			if (v == GLOBAL_SYS_EXIT_NAME) {
				REQUIRE(data.initialize_from_string(
						std::to_string(k)) == 0);
				set = true;
				break;
			}
		}
		REQUIRE(set);
		REQUIRE(data.is_enter == false);
		REQUIRE(data.sys_nr == -1);
	}
	SECTION("fork enter")
	{
		auto &sys_name_to_nr =
			std::get<0>(get_global_syscall_id_table());
		REQUIRE(sys_name_to_nr.contains("fork"));

		for (auto &[k, v] : tp_table) {
			if (v == "sys_enter_fork") {
				REQUIRE(data.initialize_from_string(
						std::to_string(k)) == 0);
				set = true;
				break;
			}
		}
		REQUIRE(set);
		REQUIRE(data.is_enter == true);
		REQUIRE(data.sys_nr == sys_name_to_nr.at("fork"));
	}
}

// aarch64 (asm-generic) defines some numbers through another macro
// (__NR_fcntl __NR3264_fcntl): the table missed them, so their tracepoints
// could not attach.
TEST_CASE("System call names map to their numbers")
{
	const auto &[name_to_id, id_to_name] = get_global_syscall_id_table();
	const std::pair<const char *, int> expected[] = {
		{ "openat", SYS_openat },     { "close", SYS_close },
		{ "fcntl", SYS_fcntl },	      { "newfstatat", SYS_newfstatat },
		{ "fstat", SYS_fstat },	      { "lseek", SYS_lseek },
		{ "mmap", SYS_mmap },	      { "statfs", SYS_statfs },
		{ "fstatfs", SYS_fstatfs },   { "truncate", SYS_truncate },
		{ "ftruncate", SYS_ftruncate }, { "sendfile", SYS_sendfile },
		{ "fadvise64", SYS_fadvise64 }, { "getdents64", SYS_getdents64 },
	};
	for (const auto &[name, nr] : expected) {
		INFO(name);
		REQUIRE(name_to_id.count(name) == 1);
		REQUIRE(name_to_id.at(name) == nr);
		REQUIRE(id_to_name.at(nr) == name);
	}
}

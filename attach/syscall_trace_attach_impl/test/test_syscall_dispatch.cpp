#include "syscall_table.hpp"
#include "syscall_trace_attach_private_data.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>
#include <syscall_trace_attach_impl.hpp>
using namespace bpftime::attach;

extern "C" int64_t _bpftime_dummy_syscall(int64_t, int64_t, int64_t, int64_t,
					  int64_t, int64_t, int64_t)
{
	return 0;
}

static int syscall_call_count;
extern "C" int64_t _bpftime_counting_syscall(int64_t, int64_t, int64_t, int64_t,
					     int64_t, int64_t, int64_t)
{
	return ++syscall_call_count;
}

TEST_CASE("Test syscall dispatch - global")
{
	syscall_trace_attach_impl attacher;
	attacher.set_original_syscall_function(_bpftime_dummy_syscall);

	syscall_trace_attach_private_data data;
	bool set = false;
	auto &tp_table = get_global_syscall_tracepoint_name_table();

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
	bool invoked = false;
	int id1 = attacher.create_attach_with_ebpf_callback(
		[&](const void *p, size_t, uint64_t *) -> int {
			invoked = true;
			auto &ctx = *(trace_event_raw_sys_enter *)p;
			REQUIRE(ctx.args[0] == 0xabcd);
			return 0;
		},
		data, ATTACH_SYSCALL_TRACE);
	REQUIRE(id1 >= 0);
	attacher.dispatch_syscall(11, 0xabcd, 0, 0, 0, 0, 0);
	REQUIRE(invoked == true);

	REQUIRE(attacher.detach_by_id(id1) == 0);
	invoked = false;
	attacher.dispatch_syscall(11, 0xabcd, 0, 0, 0, 0, 0);
	REQUIRE(invoked == false);
}

TEST_CASE("Test syscall dispatch - multiple")
{
	syscall_trace_attach_impl attacher;
	attacher.set_original_syscall_function(_bpftime_dummy_syscall);

	syscall_trace_attach_private_data data_global_enter, data_read_enter,
		data_write_enter, data_read_exit;
	int set_cnt = 0;
	auto &tp_table = get_global_syscall_tracepoint_name_table();

	auto &sys_name_to_nr = std::get<0>(get_global_syscall_id_table());
	REQUIRE(sys_name_to_nr.contains("read"));
	REQUIRE(sys_name_to_nr.contains("write"));

	for (auto &[k, v] : tp_table) {
		if (v == GLOBAL_SYS_ENTER_NAME) {
			REQUIRE(data_global_enter.initialize_from_string(
					std::to_string(k)) == 0);
			REQUIRE(data_global_enter.is_enter == true);
			REQUIRE(data_global_enter.sys_nr == -1);
			set_cnt++;
		} else if (v == "sys_enter_read") {
			REQUIRE(data_read_enter.initialize_from_string(
					std::to_string(k)) == 0);
			REQUIRE(data_read_enter.is_enter == true);
			REQUIRE(data_read_enter.sys_nr ==
				sys_name_to_nr.at("read"));
			set_cnt++;

		} else if (v == "sys_enter_write") {
			REQUIRE(data_write_enter.initialize_from_string(
					std::to_string(k)) == 0);

			REQUIRE(data_write_enter.is_enter == true);
			REQUIRE(data_write_enter.sys_nr ==
				sys_name_to_nr.at("write"));
			set_cnt++;
		} else if (v == "sys_exit_read") {
			REQUIRE(data_read_exit.initialize_from_string(
					std::to_string(k)) == 0);

			REQUIRE(data_read_exit.is_enter == false);
			REQUIRE(data_read_exit.sys_nr ==
				sys_name_to_nr.at("read"));
			set_cnt++;
		}
	}
	REQUIRE(set_cnt == 4);
	int invoke_cnt = 0;
	int id_global_enter = attacher.create_attach_with_ebpf_callback(
		[&](const void *p, size_t, uint64_t *) -> int {
			invoke_cnt++;
			return 0;
		},
		data_global_enter, ATTACH_SYSCALL_TRACE);
	REQUIRE(id_global_enter >= 0);

	int id_read_enter = attacher.create_attach_with_ebpf_callback(
		[&](const void *p, size_t, uint64_t *) -> int {
			invoke_cnt++;
			return 0;
		},
		data_read_enter, ATTACH_SYSCALL_TRACE);
	REQUIRE(id_read_enter >= 0);

	int id_write_enter = attacher.create_attach_with_ebpf_callback(
		[&](const void *p, size_t, uint64_t *) -> int {
			invoke_cnt++;
			return 0;
		},
		data_write_enter, ATTACH_SYSCALL_TRACE);
	REQUIRE(id_write_enter >= 0);

	int id_read_exit = attacher.create_attach_with_ebpf_callback(
		[&](const void *p, size_t, uint64_t *) -> int {
			invoke_cnt++;
			return 0;
		},
		data_read_exit, ATTACH_SYSCALL_TRACE);
	REQUIRE(id_read_exit >= 0);

	SECTION("call ton read")
	{
		// Dispatch a call to read
		attacher.dispatch_syscall(sys_name_to_nr.at("read"), 0, 0, 0, 0,
					  0, 0);
		REQUIRE(invoke_cnt == 3);
	}
	SECTION("call to write")
	{
		// Dispatch a call to write
		attacher.dispatch_syscall(sys_name_to_nr.at("write"), 0, 0, 0,
					  0, 0, 0);
		REQUIRE(invoke_cnt == 2);
	}
	SECTION("call to fork")
	{
		// Dispatch a call to fork
		attacher.dispatch_syscall(sys_name_to_nr.at("fork"), 0, 0, 0, 0,
					  0, 0);
		REQUIRE(invoke_cnt == 1);
	}

	REQUIRE(attacher.detach_by_id(id_global_enter) == 0);
	invoke_cnt = 0;
	attacher.dispatch_syscall(sys_name_to_nr.at("read"), 0, 0, 0, 0, 0, 0);
	REQUIRE(invoke_cnt == 2);

	REQUIRE(attacher.detach_by_id(id_read_enter) == 0);
	REQUIRE(attacher.detach_by_id(id_read_exit) == 0);
	invoke_cnt = 0;
	attacher.dispatch_syscall(sys_name_to_nr.at("read"), 0, 0, 0, 0, 0, 0);
	REQUIRE(invoke_cnt == 0);

	invoke_cnt = 0;
	attacher.dispatch_syscall(sys_name_to_nr.at("write"), 0, 0, 0, 0, 0, 0);
	REQUIRE(invoke_cnt == 1);

	REQUIRE(attacher.detach_by_id(id_write_enter) == 0);
	invoke_cnt = 0;
	attacher.dispatch_syscall(sys_name_to_nr.at("write"), 0, 0, 0, 0, 0, 0);
	REQUIRE(invoke_cnt == 0);
}

TEST_CASE("A failing exit callback is contained")
{
	syscall_trace_attach_private_data data;
	data.sys_nr = 11;
	data.is_enter = false;
	syscall_trace_attach_impl attacher;
	attacher.set_original_syscall_function(_bpftime_counting_syscall);
	REQUIRE(attacher.create_attach_with_ebpf_callback(
			[&](const void *, size_t, uint64_t *) -> int {
				throw std::runtime_error(
					"test callback failure");
			},
			data, ATTACH_SYSCALL_TRACE) >= 0);
	int later_callback_count = 0;
	data.sys_nr = -1;
	REQUIRE(attacher.create_attach_with_ebpf_callback(
			[&](const void *, size_t, uint64_t *) -> int {
				return ++later_callback_count;
			},
			data, ATTACH_SYSCALL_TRACE) >= 0);
	syscall_call_count = 0;
	REQUIRE(attacher.dispatch_syscall(11, 0, 0, 0, 0, 0, 0) == 1);
	REQUIRE(syscall_call_count == 1);
	REQUIRE(later_callback_count == 1);
	REQUIRE(!curr_thread_override_return_callback.has_value());
}

// Counts operator new calls on this thread while enabled. The dispatcher runs
// for every system call of the traced process, so it must not allocate.
static thread_local bool count_allocations = false;
static thread_local int allocation_count = 0;

void *operator new(std::size_t size)
{
	if (count_allocations)
		allocation_count++;
	if (void *p = std::malloc(size ? size : 1))
		return p;
	throw std::bad_alloc();
}

void operator delete(void *p) noexcept
{
	std::free(p);
}

void operator delete(void *p, std::size_t) noexcept
{
	std::free(p);
}

static int override_callback_seen;
extern "C" int64_t _bpftime_checking_syscall(int64_t, int64_t, int64_t,
					     int64_t, int64_t, int64_t, int64_t)
{
	if (curr_thread_override_return_callback.has_value())
		override_callback_seen++;
	return ++syscall_call_count;
}

static int attach_counter(syscall_trace_attach_impl &attacher, int sys_nr,
			  bool is_enter, int &counter)
{
	syscall_trace_attach_private_data data;
	data.sys_nr = sys_nr;
	data.is_enter = is_enter;
	return attacher.create_attach_with_ebpf_callback(
		[&counter](const void *, size_t, uint64_t *) -> int {
			counter++;
			return 0;
		},
		data, ATTACH_SYSCALL_TRACE);
}

TEST_CASE("Dispatch of a system call without programs passes it through")
{
	syscall_trace_attach_impl attacher;
	attacher.set_original_syscall_function(_bpftime_checking_syscall);
	int other_enter = 0, other_exit = 0;
	REQUIRE(attach_counter(attacher, 20, true, other_enter) >= 0);
	REQUIRE(attach_counter(attacher, 21, false, other_exit) >= 0);

	syscall_call_count = 0;
	override_callback_seen = 0;
	allocation_count = 0;
	count_allocations = true;
	int64_t ret = 0;
	for (int i = 0; i < 100; i++)
		ret = attacher.dispatch_syscall(11, 0, 0, 0, 0, 0, 0);
	count_allocations = false;
	REQUIRE(ret == 100);
	REQUIRE(syscall_call_count == 100);
	REQUIRE(allocation_count == 0);
	REQUIRE(override_callback_seen == 0);
	REQUIRE(other_enter == 0);
	REQUIRE(other_exit == 0);
	REQUIRE(!curr_thread_override_return_callback.has_value());
}

TEST_CASE("Dispatch with programs does not allocate")
{
	syscall_trace_attach_impl attacher;
	attacher.set_original_syscall_function(_bpftime_counting_syscall);
	int enter = 0, exits = 0, global_enter = 0, global_exit = 0;
	REQUIRE(attach_counter(attacher, 11, true, enter) >= 0);
	REQUIRE(attach_counter(attacher, 11, false, exits) >= 0);
	REQUIRE(attach_counter(attacher, -1, true, global_enter) >= 0);
	REQUIRE(attach_counter(attacher, -1, false, global_exit) >= 0);

	syscall_call_count = 0;
	allocation_count = 0;
	count_allocations = true;
	for (int i = 0; i < 100; i++)
		attacher.dispatch_syscall(11, 0, 0, 0, 0, 0, 0);
	count_allocations = false;
	REQUIRE(syscall_call_count == 100);
	REQUIRE(enter == 100);
	REQUIRE(exits == 100);
	REQUIRE(global_enter == 100);
	REQUIRE(global_exit == 100);
	REQUIRE(allocation_count == 0);
	REQUIRE(!curr_thread_override_return_callback.has_value());
}

TEST_CASE("Out-of-range system call numbers run only global programs")
{
	syscall_trace_attach_impl attacher;
	attacher.set_original_syscall_function(_bpftime_counting_syscall);
	int global_exit = 0, exit_88 = 0;
	REQUIRE(attach_counter(attacher, -1, false, global_exit) >= 0);
	REQUIRE(attach_counter(attacher, 88, false, exit_88) >= 0);

	// syscall(-1) probes for ENOSYS; index -1 of the per-syscall lists is
	// another member of the attacher.
	syscall_call_count = 0;
	REQUIRE(attacher.dispatch_syscall(-1, 0, 0, 0, 0, 0, 0) == 1);
	REQUIRE(global_exit == 1);
	REQUIRE(exit_88 == 0);

	// x32 numbers (0x40000000 + nr) and others above the lists: run in a
	// child, as the lookup reads past the attacher.
	for (int64_t nr : { (int64_t)512 + 88, (int64_t)0x40000000 + 1 }) {
		fflush(nullptr);
		pid_t pid = fork();
		REQUIRE(pid >= 0);
		if (pid == 0) {
			global_exit = exit_88 = 0;
			syscall_call_count = 0;
			attacher.dispatch_syscall(nr, 0, 0, 0, 0, 0, 0);
			_exit(syscall_call_count == 1 && global_exit == 1 &&
					      exit_88 == 0 ?
				      0 :
				      1);
		}
		int status = 0;
		REQUIRE(waitpid(pid, &status, 0) == pid);
		INFO("system call number " << nr << ", status " << status);
		REQUIRE(WIFEXITED(status));
		REQUIRE(WEXITSTATUS(status) == 0);
	}
}

TEST_CASE("Programs override the return value of a system call")
{
	syscall_trace_attach_impl attacher;
	attacher.set_original_syscall_function(_bpftime_counting_syscall);
	syscall_trace_attach_private_data data;
	data.sys_nr = 11;
	int exits = 0;

	SECTION("at entry, skipping the system call")
	{
		data.is_enter = true;
		REQUIRE(attacher.create_attach_with_ebpf_callback(
				[](const void *, size_t, uint64_t *) -> int {
					bpftime_override_return(0, 77);
					return 0;
				},
				data, ATTACH_SYSCALL_TRACE) >= 0);
		REQUIRE(attach_counter(attacher, 11, false, exits) >= 0);
		syscall_call_count = 0;
		REQUIRE(attacher.dispatch_syscall(11, 0, 0, 0, 0, 0, 0) == 77);
		REQUIRE(syscall_call_count == 0);
		REQUIRE(exits == 0);
	}
	SECTION("at exit")
	{
		data.is_enter = false;
		REQUIRE(attacher.create_attach_with_ebpf_callback(
				[](const void *, size_t, uint64_t *) -> int {
					bpftime_set_retval(99);
					return 0;
				},
				data, ATTACH_SYSCALL_TRACE) >= 0);
		syscall_call_count = 0;
		REQUIRE(attacher.dispatch_syscall(11, 0, 0, 0, 0, 0, 0) == 99);
		REQUIRE(syscall_call_count == 1);
		// Without a program on this system call nothing is overridden.
		REQUIRE(attacher.dispatch_syscall(12, 0, 0, 0, 0, 0, 0) == 2);
	}
	REQUIRE(!curr_thread_override_return_callback.has_value());
}

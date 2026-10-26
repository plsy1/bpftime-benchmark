/* SPDX-License-Identifier: MIT */
// Tests that load the daemon's kernel tracer; they need root and skip
// otherwise.
#include <catch2/catch_test_macros.hpp>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <fcntl.h>
#include <linux/perf_event.h>
#include <cstring>
#include <fstream>
#include <memory>
#include <sys/syscall.h>
#include <unistd.h>
#include "bpf_tracer_event.h"
#include "user/daemon.hpp"
#include "user/bpf_prog_insns.hpp"
#include "bpf_tracer.skel.h"

namespace
{
struct tracer_deleter {
	void operator()(bpf_tracer_bpf *obj) const
	{
		bpf_tracer_bpf__destroy(obj);
	}
};
using tracer_ptr = std::unique_ptr<bpf_tracer_bpf, tracer_deleter>;

// Opens the tracer filtered to this process, with the programs the daemon
// would select for ENV.
tracer_ptr open_tracer(const daemon_config &env)
{
	tracer_ptr obj(bpf_tracer_bpf__open());
	REQUIRE(obj);
	obj->rodata->target_pid = getpid();
	bpftime::select_tracer_programs(obj.get(), env);
	return obj;
}
} // namespace

TEST_CASE("Daemon tracer attaches without the syscalls this kernel lacks",
	  "[daemon-tracer]")
{
	if (geteuid() != 0)
		SKIP("loading the tracer needs root");
	daemon_config env;
	// -o adds sys_enter_open/sys_exit_open, which arm64 and riscv64 do not
	// have; attaching them used to fail the whole tracer there.
	env.show_open = true;
	auto obj = open_tracer(env);
	REQUIRE(bpf_tracer_bpf__load(obj.get()) == 0);
	REQUIRE(bpf_tracer_bpf__attach(obj.get()) == 0);
	REQUIRE(bpf_program__autoload(
		obj->progs.tracepoint__syscalls__sys_enter_openat));
	REQUIRE(bpf_program__autoload(
		obj->progs.tracepoint__syscalls__sys_enter_bpf));
}

namespace
{
// The tracer's pid/fd table entry for FD of this process, or type
// BPF_FD_TYPE_MAX when there is none.
bpf_fd_data fd_entry(bpf_tracer_bpf *obj, int fd)
{
	unsigned long long key = MAKE_PFD(getpid(), fd);
	bpf_fd_data data = {};
	if (bpf_map__lookup_elem(obj->maps.bpf_fd_map, &key, sizeof(key), &data,
				 sizeof(data), 0) != 0)
		data.type = BPF_FD_TYPE_MAX;
	return data;
}

unsigned map_id(int fd)
{
	bpf_map_info info = {};
	__u32 len = sizeof(info);
	REQUIRE(bpf_obj_get_info_by_fd(fd, &info, &len) == 0);
	return info.id;
}
} // namespace

TEST_CASE("Daemon tracer follows duplicated bpf descriptors", "[daemon-tracer]")
{
	if (geteuid() != 0)
		SKIP("loading the tracer needs root");
	daemon_config env;
	auto obj = open_tracer(env);
	REQUIRE(bpf_tracer_bpf__load(obj.get()) == 0);
	REQUIRE(bpf_tracer_bpf__attach(obj.get()) == 0);

	int map_fd = bpf_map_create(BPF_MAP_TYPE_ARRAY, "dup_test", 4, 8, 1,
				    nullptr);
	REQUIRE(map_fd >= 0);
	const unsigned id = map_id(map_fd);
	REQUIRE(fd_entry(obj.get(), map_fd).type == BPF_FD_TYPE_MAP);
	REQUIRE(fd_entry(obj.get(), map_fd).kernel_id == (int)id);

	SECTION("dup2 onto a reserved descriptor (libbpf's reuse_fd)")
	{
		// dup2 is the dup3 system call on arm64, dup2 on x86-64.
		int reserved = open("/dev/null", O_RDONLY | O_CLOEXEC);
		// Opened before map_fd is closed: the tracer keeps entries of
		// closed bpf fds (it resolves them asynchronously), so a reused
		// number would still look like a map.
		int null_fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
		REQUIRE(reserved >= 0);
		REQUIRE(null_fd >= 0);
		REQUIRE(dup2(map_fd, reserved) == reserved);
		close(map_fd);
		map_fd = -1;
		auto entry = fd_entry(obj.get(), reserved);
		CHECK(entry.type == BPF_FD_TYPE_MAP);
		CHECK(entry.kernel_id == (int)id);
		// A non-bpf descriptor duplicated over it clears the entry.
		REQUIRE(dup2(null_fd, reserved) == reserved);
		CHECK(fd_entry(obj.get(), reserved).type == BPF_FD_TYPE_MAX);
		close(null_fd);
		close(reserved);
	}
	SECTION("fcntl F_DUPFD_CLOEXEC")
	{
		int copy = fcntl(map_fd, F_DUPFD_CLOEXEC, 100);
		REQUIRE(copy >= 100);
		CHECK(fd_entry(obj.get(), copy).type == BPF_FD_TYPE_MAP);
		CHECK(fd_entry(obj.get(), copy).kernel_id == (int)id);
		close(copy);
	}
	SECTION("dup")
	{
		int copy = dup(map_fd);
		REQUIRE(copy >= 0);
		CHECK(fd_entry(obj.get(), copy).type == BPF_FD_TYPE_MAP);
		CHECK(fd_entry(obj.get(), copy).kernel_id == (int)id);
		close(copy);
	}
	if (map_fd >= 0)
		close(map_fd);
}

TEST_CASE("Daemon tracer restores only the redirected uprobe path",
	  "[daemon-tracer]")
{
	if (geteuid() != 0)
		SKIP("loading the tracer needs root");
	int uprobe_type = -1;
	std::ifstream("/sys/bus/event_source/devices/uprobe/type") >>
		uprobe_type;
	REQUIRE(uprobe_type >= 0);

	daemon_config env;
	tracer_ptr obj(bpf_tracer_bpf__open());
	REQUIRE(obj);
	obj->rodata->target_pid = getpid();
	obj->rodata->enable_replace_uprobe = true;
	obj->rodata->uprobe_perf_type = uprobe_type;
	strncpy(obj->rodata->new_uprobe_path, "/a", PATH_LENTH);
	bpftime::select_tracer_programs(obj.get(), env);
	REQUIRE(bpf_tracer_bpf__load(obj.get()) == 0);
	REQUIRE(bpf_tracer_bpf__attach(obj.get()) == 0);

	// The loader's path buffer holds exactly the string (as a strdup'ed
	// path in DeepFlow does); what follows it must not be touched.
	const char path[] = "/proc/self/exe";
	unsigned char buf[sizeof(path) + 64];
	memcpy(buf, path, sizeof(path));
	memset(buf + sizeof(path), 0xa5, sizeof(buf) - sizeof(path));
	perf_event_attr attr = {};
	attr.size = sizeof(attr);
	attr.type = uprobe_type;
	attr.uprobe_path = (__u64)(uintptr_t)buf;
	attr.probe_offset = 0x1000;
	int fd = syscall(SYS_perf_event_open, &attr, getpid(), -1, -1, 0);
	if (fd >= 0)
		close(fd);

	CHECK(std::string((const char *)buf) == path);
	CHECK(attr.probe_offset == 0x1000);
	bool guard_intact = true;
	for (size_t i = sizeof(path); i < sizeof(buf); i++)
		guard_intact = guard_intact && buf[i] == 0xa5;
	CHECK(guard_intact);
}

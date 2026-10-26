/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <bpf/bpf.h>
#include <unistd.h>
#include "bpftime_shm.hpp"
#include "bpftime_shm_internal.hpp"
#include "handler/link_handler.hpp"
#include "user/bpf_prog_insns.hpp"
#include "user/bpftime_driver.hpp"
#if !defined(__x86_64__) && defined(_M_X64)
#error Only supports x86_64
#endif

TEST_CASE("Daemon driver preserves perf-link cookies")
{
	constexpr int server_pid = 1234;
	constexpr int perf_fd = 7;
	constexpr int prog_fd = 8;
	constexpr uint64_t cookie = 0x12345678;
	bpftime::bpftime_driver driver({}, nullptr);
	ebpf_inst insn = {};

	REQUIRE(bpftime_progs_create(
			prog_fd, &insn, 1, "cookie_test",
			static_cast<int>(
				bpftime::bpf_prog_type::BPF_PROG_TYPE_KPROBE)) >=
		0);
	driver.adopt_prog_handle(prog_fd, prog_fd);
	REQUIRE(driver.bpftime_uprobe_create_server(server_pid, perf_fd, 0,
						    "/proc/self/exe", 0, false,
						    0) >= 0);

	REQUIRE(driver.bpftime_attach_perf_to_bpf_server(server_pid, perf_fd,
							 prog_fd, cookie) >= 0);
	const auto *manager =
		bpftime::shm_holder.global_shared_memory.get_manager();
	bool found = false;
	for (std::size_t i = 0; i < manager->size(); i++) {
		if (auto *link = std::get_if<bpftime::bpf_link_handler>(
			    &manager->get_handler(i));
		    link != nullptr && link->attach_cookie == cookie) {
			found = true;
		}
	}
	REQUIRE(found);
}

TEST_CASE("Daemon rejects instruction counts beyond the captured buffer")
{
	bpf_insn_data captured = {};
	captured.code_len = BPF_COMPLEXITY_LIMIT_INSNS + 1;
	std::vector<ebpf_inst> insns(1);

	const int result = bpftime::detail::copy_captured_bpf_prog_insns(
		insns, captured);
	REQUIRE(result == -E2BIG);
	REQUIRE(insns.empty());
}

TEST_CASE("Daemon accepts the exact captured instruction capacity")
{
	bpf_insn_data captured = {};
	captured.code_len = BPF_COMPLEXITY_LIMIT_INSNS;
	auto *captured_insns = reinterpret_cast<ebpf_inst *>(captured.code);
	captured_insns[0].code = EBPF_OP_MOV64_IMM;
	captured_insns[0].imm = 7;
	captured_insns[BPF_COMPLEXITY_LIMIT_INSNS - 1].code = EBPF_OP_EXIT;
	std::vector<ebpf_inst> insns;

	REQUIRE(bpftime::detail::copy_captured_bpf_prog_insns(insns,
							     captured) == 0);
	REQUIRE(insns.size() == BPF_COMPLEXITY_LIMIT_INSNS);
	REQUIRE(insns.front().code == EBPF_OP_MOV64_IMM);
	REQUIRE(insns.front().imm == 7);
	REQUIRE(insns.back().code == EBPF_OP_EXIT);
}

// DeepFlow's goroutine tracking shares an LRU hash (go_ancerstor_map) with
// kernel programs; the daemon must mirror it ("Unsupported map type: 1009").
TEST_CASE("Daemon mirrors kernel LRU hash maps", "[daemon-tracer]")
{
	if (geteuid() != 0)
		SKIP("creating a kernel map needs root");
	int fd = bpf_map_create(BPF_MAP_TYPE_LRU_HASH, "lru_mirror", 4, 8, 16,
				nullptr);
	REQUIRE(fd >= 0);
	bpf_map_info info = {};
	uint32_t len = sizeof(info);
	REQUIRE(bpf_obj_get_info_by_fd(fd, &info, &len) == 0);
	uint32_t key = 3;
	uint64_t value = 0x1122334455667788;
	REQUIRE(bpf_map_update_elem(fd, &key, &value, BPF_ANY) == 0);

	bpftime::bpftime_driver driver({}, nullptr);
	const int handle = driver.bpftime_maps_create_server(info.id);
	REQUIRE(handle >= 0);
	REQUIRE(bpftime_is_map_fd(handle));
	auto *seen = (const uint64_t *)bpftime_map_lookup_elem(handle, &key);
	REQUIRE(seen != nullptr);
	CHECK(*seen == value);
	close(fd);
}

// Kernel ids are global and grow without bound (after a few DeepFlow runs on
// this machine they exceeded the 6144-entry handler table: "set_handler: fd
// 6957 out of range"), and maps and programs have separate id spaces. The
// daemon mirrors each kernel object at a free handler index, once.
TEST_CASE("Daemon mirrors kernel maps at free handler indexes",
	  "[daemon-tracer]")
{
	if (geteuid() != 0)
		SKIP("creating a kernel map needs root");
	int fd1 = bpf_map_create(BPF_MAP_TYPE_HASH, "handle_a", 4, 4, 4,
				 nullptr);
	int fd2 = bpf_map_create(BPF_MAP_TYPE_HASH, "handle_b", 4, 4, 4,
				 nullptr);
	REQUIRE(fd1 >= 0);
	REQUIRE(fd2 >= 0);
	bpf_map_info info1 = {}, info2 = {};
	uint32_t len = sizeof(info1);
	REQUIRE(bpf_obj_get_info_by_fd(fd1, &info1, &len) == 0);
	len = sizeof(info2);
	REQUIRE(bpf_obj_get_info_by_fd(fd2, &info2, &len) == 0);

	bpftime::bpftime_driver driver({}, nullptr);
	const int free_before = bpftime_find_minimal_unused_fd();
	const int h1 = driver.bpftime_maps_create_server(info1.id);
	REQUIRE(h1 == free_before);
	const int h2 = driver.bpftime_maps_create_server(info2.id);
	REQUIRE(h2 >= 0);
	CHECK(h2 != h1);
	// Mirrored once: asking again returns the same handle.
	CHECK(driver.bpftime_maps_create_server(info1.id) == h1);
	CHECK(bpftime_is_map_fd(h1));
	CHECK(bpftime_is_map_fd(h2));
	close(fd1);
	close(fd2);
}

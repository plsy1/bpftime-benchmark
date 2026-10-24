/* SPDX-License-Identifier: MIT */
#include <catch2/catch_test_macros.hpp>
#include <bpftime_prog.hpp>
#include <bpftime_shm_internal.hpp>
#include <bpftime_helper_group.hpp>
#include <bpf_map/array_map_binding.hpp>
#include <linux/bpf.h>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <memory>
#include <thread>
#include <sys/wait.h>
#include <unistd.h>

namespace {
using namespace bpftime;
struct isolated_shm {
	std::string name = "bpftime-array-jit-lifetime-" + std::to_string(getpid());
	bool had_name = getenv("BPFTIME_GLOBAL_SHM_NAME") != nullptr;
	std::string previous = had_name ? getenv("BPFTIME_GLOBAL_SHM_NAME") : "";
	isolated_shm()
	{
		bpftime_destroy_global_shm();
		REQUIRE(setenv("BPFTIME_GLOBAL_SHM_NAME", name.c_str(), 1) == 0);
		bpftime_initialize_global_shm(shm_open_type::SHM_REMOVE_AND_CREATE);
		runtime_config config;
		config.set_vm_name("llvm");
		shm_holder.global_shared_memory.set_runtime_config(std::move(config));
	}
	~isolated_shm()
	{
		shm_holder.global_shared_memory.reset_server_state();
		bpftime_destroy_global_shm();
		boost::interprocess::shared_memory_object::remove(name.c_str());
		bpftime_initialize_global_shm(shm_open_type::SHM_NO_CREATE);
		if (had_name)
			setenv("BPFTIME_GLOBAL_SHM_NAME", previous.c_str(), 1);
		else
			unsetenv("BPFTIME_GLOBAL_SHM_NAME");
	}
};

int create_array(uint64_t value, uint32_t capacity = 4)
{
	constexpr int fd = 100;
	REQUIRE(bpftime_maps_create(fd, "array_jit_lifetime", {
		.type = BPF_MAP_TYPE_ARRAY, .key_size = 4, .value_size = 8,
		.max_ents = capacity}) == fd);
	for (uint32_t key = 0; key < capacity; ++key)
		REQUIRE(bpftime_map_update_elem(fd, &key, &value, BPF_ANY) == 0);
	return fd;
}

std::unique_ptr<bpftime_prog> make_program(int fd, bool request = true,
	void *helper = reinterpret_cast<void *>(bpftime_array_map_lookup_elem_helper),
	bool dynamic = false)
{
	std::vector<ebpf_inst> code = {
		{0xbf, 2, 1, 0, 0},
		{0x18, 1, BPF_PSEUDO_MAP_FD, 0, fd},
		{0x00, 0, 0, 0, 0},
		{0x85, 0, 0, 0, BPF_FUNC_map_lookup_elem},
		{0x95, 0, 0, 0, 0},
	};
	if (dynamic) {
		// Keep a declared map to specialize, but select the actual handle at
		// runtime. The helper must retain a transient map through execution.
		code.insert(code.begin() + 3, ebpf_inst{0x79, 1, 2, 8, 0});
	}
	auto prog = std::make_unique<bpftime_prog>(code.data(), code.size(), "array_jit_lifetime");
	if (request)
		REQUIRE(prog->request_array_map_inlining() == 0);
	REQUIRE(prog->bpftime_prog_register_raw_helper({1, "bpf_map_lookup_elem", helper}) == 0);
	REQUIRE(prog->bpftime_prog_load(true) == 0);
	return prog;
}

uint64_t lookup(bpftime_prog &prog, uint32_t key, int expected_errno = EDOM)
{
	errno = EDOM;
	uint64_t result = 0;
	REQUIRE(prog.bpftime_prog_exec(&key, sizeof(key), &result) == 0);
	REQUIRE(errno == expected_errno);
	return result;
}
uint64_t custom_lookup(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)
{
	return 12345;
}
}

TEST_CASE("Array JIT owns map identity after close, reuse and server reset",
	"[array-jit-lifetime]")
{
	isolated_shm fixture;
	int fd = create_array(42);
	auto prog = make_program(fd);
	auto address = lookup(*prog, 2);
	REQUIRE(*reinterpret_cast<uint64_t *>(address) == 42);
	bpftime_close(fd);
	create_array(99, 8);
	REQUIRE(lookup(*prog, 2) == address);
	REQUIRE(*reinterpret_cast<uint64_t *>(address) == 42);
	// A new map at the same FD accepts key 6, but the pinned original must not.
	REQUIRE(lookup(*prog, 6, ENOENT) == 0);
	shm_holder.global_shared_memory.begin_new_session();
	REQUIRE(lookup(*prog, 2) == address);
	REQUIRE(lookup(*prog, UINT32_MAX, ENOENT) == 0);
	REQUIRE(prog->bpftime_prog_register_raw_helper({1,"custom",(void *)custom_lookup}) == -EBUSY);
	REQUIRE(prog->prepare_for_signal_execution() == -ENOTSUP);
	REQUIRE(prog->load_aot_object({}) == -ENOTSUP);
	REQUIRE(prog->bpftime_prog_load(true) == -EBUSY);
}

TEST_CASE("Array JIT retains exact mapping across unmap, unlink and recreate",
	"[array-jit-lifetime]")
{
	isolated_shm fixture;
	int fd = create_array(123);
	auto prog = make_program(fd);
	auto address = lookup(*prog, 1);
	shm_holder.global_shared_memory.reset_server_state();
	bpftime_destroy_global_shm();
	bpftime_initialize_global_shm(shm_open_type::SHM_REMOVE_AND_CREATE);
	create_array(777, 8);
	REQUIRE(lookup(*prog, 1) == address);
	REQUIRE(*reinterpret_cast<uint64_t *>(address) == 123);
	REQUIRE(lookup(*prog, 6, ENOENT) == 0);
	prog.reset(); // Must free through the old retained allocator, not the new one.
}

TEST_CASE("Array JIT shares backing with dup and releases last reference",
	"[array-jit-lifetime]")
{
	isolated_shm fixture;
	auto memory = shm_holder.global_shared_memory.retain_mapping();
	(void)map_binding_mutex(*memory); // Exclude the persistent mutex allocation.
	auto before = memory->get_free_memory();
	int fd = create_array(5);
	auto prog = make_program(fd);
	REQUIRE(shm_holder.global_shared_memory.dup_bpf_map(fd, fd) == fd);
	REQUIRE(shm_holder.global_shared_memory.dup_bpf_map(fd, 101) == 101);
	bpftime_close(fd);
	uint32_t key = 1;
	uint64_t changed = 987;
	REQUIRE(bpftime_map_update_elem(101, &key, &changed, BPF_ANY) == 0);
	REQUIRE(*reinterpret_cast<uint64_t *>(lookup(*prog, key)) == changed);
	bpftime_close(101);
	REQUIRE(*reinterpret_cast<uint64_t *>(lookup(*prog, key)) == changed);
	prog.reset();
	REQUIRE(memory->get_free_memory() == before);
}

TEST_CASE("Array JIT execution tolerates concurrent close and FD replacement",
	"[array-jit-lifetime]")
{
	isolated_shm fixture;
	int fd = create_array(456);
	auto prog = make_program(fd);
	std::atomic<bool> start{false};
	std::atomic<unsigned> failures{0};
	std::thread reader([&] {
		while (!start.load(std::memory_order_acquire)) {}
		for (unsigned i = 0; i < 20000; ++i) {
			uint32_t key = i % 4;
			uint64_t result = 0;
			if (prog->bpftime_prog_exec(&key, sizeof(key), &result) != 0 ||
			    result == 0 || *reinterpret_cast<uint64_t *>(result) != 456)
				++failures;
		}
	});
	start.store(true, std::memory_order_release);
	for (unsigned i = 0; i < 200; ++i) {
		bpftime_close(fd);
		create_array(i + 1000, 8);
	}
	reader.join();
	REQUIRE(failures == 0);
}

TEST_CASE("Array JIT references survive another process closing the FD",
	"[array-jit-lifetime]")
{
	isolated_shm fixture;
	int fd = create_array(73);
	auto prog = make_program(fd);
	pid_t child = fork();
	REQUIRE(child >= 0);
	if (child == 0) {
		bpftime_close(fd);
		_exit(0); // Do not destruct the parent's inherited C++ ownership graph.
	}
	int status = -1;
	REQUIRE(waitpid(child, &status, 0) == child);
	REQUIRE(WIFEXITED(status));
	REQUIRE(WEXITSTATUS(status) == 0);
	REQUIRE(*reinterpret_cast<uint64_t *>(lookup(*prog, 0)) == 73);
	REQUIRE(lookup(*prog, 4, ENOENT) == 0);
}

TEST_CASE("Array JIT request leaves interpreter and uBPF on original path",
	"[array-jit-lifetime]")
{
	isolated_shm fixture;
	int fd = create_array(31);
	for (const char *backend : {"llvm", "ubpf"}) {
		runtime_config config;
		config.set_vm_name(backend);
		shm_holder.global_shared_memory.set_runtime_config(std::move(config));
		ebpf_inst code[] = {
			{0xbf,2,1,0,0}, {0x18,1,BPF_PSEUDO_MAP_FD,0,fd},
			{0,0,0,0,0}, {0x85,0,0,0,1}, {0x95,0,0,0,0},
		};
		bpftime_prog prog(code,5,"array_jit_interpreter_fallback");
		REQUIRE(prog.request_array_map_inlining() == 0);
		REQUIRE(prog.bpftime_prog_register_raw_helper({1,"bpf_map_lookup_elem",
			(void *)bpftime_array_map_lookup_elem_helper}) == 0);
		REQUIRE(prog.bpftime_prog_load(false) == 0);
		uint32_t key = 1;
		uint64_t result = 0;
		REQUIRE(prog.bpftime_prog_exec(&key,sizeof(key),&result) == 0);
		REQUIRE(result != 0);
		REQUIRE(*reinterpret_cast<uint64_t *>(result) == 31);
		int expected = std::string(backend) == "ubpf" ? 0 : -EEXIST;
		REQUIRE(prog.bpftime_prog_register_raw_helper({1,"custom",(void *)custom_lookup}) == expected);
	}
}

TEST_CASE("Array JIT keeps custom helpers and dynamic lookup fallback",
	"[array-jit-lifetime]")
{
	isolated_shm fixture;
	int fd = create_array(17);
	auto custom = make_program(fd, true, (void *)custom_lookup);
	REQUIRE(lookup(*custom, 0) == 12345);
	auto dynamic = make_program(fd, true, (void *)bpftime_array_map_lookup_elem_helper, true);
	struct {uint32_t key, padding; uint64_t fd;} input{2,0,100};
	uint64_t result = 0;
	REQUIRE(dynamic->bpftime_prog_exec(&input,sizeof(input),&result) == 0);
	REQUIRE(*reinterpret_cast<uint64_t *>(result) == 17);
	// A different, runtime-selected array is pinned by the slow helper too.
	REQUIRE(bpftime_maps_create(102,"dynamic_array",{.type=BPF_MAP_TYPE_ARRAY,
		.key_size=4,.value_size=8,.max_ents=4}) == 102);
	uint64_t value = 91;
	REQUIRE(bpftime_map_update_elem(102,&input.key,&value,BPF_ANY) == 0);
	input.fd = 102;
	auto memory = shm_holder.global_shared_memory.retain_mapping();
	auto before = memory->get_free_memory();
	REQUIRE(dynamic->bpftime_prog_exec(&input,sizeof(input),&result) == 0);
	REQUIRE(*reinterpret_cast<uint64_t *>(result) == 91);
	REQUIRE(memory->get_free_memory() == before);
}

TEST_CASE("Array JIT binds implementation metadata, not duplicate attributes",
	"[array-jit-lifetime]")
{
	isolated_shm fixture;
	int fd = create_array(88);
	const auto *manager = shm_holder.global_shared_memory.get_manager();
	// The legacy explicit-field handler constructor does not populate attr.
	// Public metadata can also be changed independently of the implementation.
	auto &map = const_cast<bpf_map_handler &>(std::get<bpf_map_handler>(manager->get_handler(fd)));
	map.attr = {};
	auto prog = make_program(fd);
	bpftime_close(fd);
	REQUIRE(*reinterpret_cast<uint64_t *>(lookup(*prog, 3)) == 88);
	REQUIRE(lookup(*prog, 4, ENOENT) == 0);
}

TEST_CASE("Array JIT descriptor failure cannot execute stale code or retry",
	"[array-jit-lifetime]")
{
	isolated_shm fixture;
	int fd = create_array(19);
	ebpf_inst code[] = {{0xbf,2,1,0,0},{0x18,1,BPF_PSEUDO_MAP_FD,0,fd},
		{0,0,0,0,0},{0x85,0,0,0,1},{0x95,0,0,0,0}};
	bpftime_prog prog(code,5,"array_jit_failed_registration");
	REQUIRE(prog.request_array_map_inlining() == 0);
	REQUIRE(prog.bpftime_prog_register_raw_helper({1,"bpf_map_lookup_elem",
		(void *)bpftime_array_map_lookup_elem_helper}) == 0);
	auto impl = shm_holder.global_shared_memory.try_get_array_map_impl(fd);
	REQUIRE(impl.has_value());
	// Simulate a partially preconfigured VM whose descriptor slot is occupied.
	REQUIRE(ebpf_register_array_map(const_cast<ebpf_vm *>(prog.get_vm()),fd,
		(uint64_t)(*impl)->get_raw_data(),4,8,8,4) == 0);
	REQUIRE(prog.bpftime_prog_load(true) == -EEXIST);
	bpftime_close(fd);
	uint32_t key = 0;
	uint64_t result = 123;
	REQUIRE(prog.bpftime_prog_exec(&key,sizeof(key),&result) == -EINVAL);
	REQUIRE(result == 0);
	REQUIRE(prog.bpftime_prog_load(true) == -EINVAL);
	REQUIRE(prog.load_aot_object({}) == -ENOTSUP);
}

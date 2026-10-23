/* SPDX-License-Identifier: MIT */
#include <catch2/catch_test_macros.hpp>
#include <bpftime_prog.hpp>
#include <bpftime_shm_internal.hpp>
#include <bpftime_helper_group.hpp>
#include <bpf_attach_ctx.hpp>
#include <bpftime_vm_compat.hpp>
#include <bpftime_internal.h>
#include <bpf_map/array_map_binding.hpp>
#include <linux/bpf.h>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <memory>
#include <thread>
#include <sys/wait.h>
#include <unistd.h>
#include <sched.h>
#include <sstream>
#include <spdlog/sinks/ostream_sink.h>

#if defined(__linux__)
namespace {
thread_local bool cpu_query_injected = false;
thread_local int cpu_query_result = 0;
thread_local unsigned cpu_query_reads = 0;
}
extern "C" int __real_sched_getcpu();
extern "C" int __wrap_sched_getcpu()
{
	if (!cpu_query_injected)
		return __real_sched_getcpu();
	++cpu_query_reads;
	if (cpu_query_result == -1)
		errno = EIO;
	return cpu_query_result;
}
#endif

namespace {
using namespace bpftime;
struct captured_array_log {
	std::ostringstream messages;
	std::shared_ptr<spdlog::logger> old = spdlog::default_logger();
	captured_array_log()
	{
		auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(messages);
		auto logger = std::make_shared<spdlog::logger>("array-default",sink);
		logger->set_level(spdlog::level::info);
		spdlog::set_default_logger(logger);
	}
	~captured_array_log() { spdlog::set_default_logger(old); }
};
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
	bool dynamic = false, void *cpu_helper = reinterpret_cast<void *>(bpftime_get_smp_processor_id))
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
	if (helper == reinterpret_cast<void *>(bpftime_percpu_array_map_lookup_elem_helper))
		REQUIRE(prog->bpftime_prog_register_raw_helper({8, "bpf_get_smp_processor_id", cpu_helper}) == 0);
	REQUIRE(prog->bpftime_prog_register_raw_helper({1, "bpf_map_lookup_elem", helper}) == 0);
	REQUIRE(prog->bpftime_prog_load(true) == 0);
	return prog;
}

uint64_t lookup(bpftime_prog &prog, uint32_t key, int expected_errno = EDOM);
uint64_t custom_lookup(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

TEST_CASE("Per-CPU array JIT follows real affinity changes and retains map identity",
	"[array-jit-lifetime][percpu-array-jit]")
{
	isolated_shm fixture;
	cpu_set_t original;
	REQUIRE(sched_getaffinity(0,sizeof(original),&original)==0);
	struct restore_affinity {
		cpu_set_t mask;
		~restore_affinity() { sched_setaffinity(0,sizeof(mask),&mask); }
	} restore{original};
	const int cpus=sysconf(_SC_NPROCESSORS_ONLN);
	REQUIRE(cpus>0);
	auto memory=shm_holder.global_shared_memory.retain_mapping();
	(void)map_binding_mutex(*memory);
	const auto free_before=memory->get_free_memory();
	constexpr int fd=100;
	REQUIRE(bpftime_maps_create(fd,"percpu_jit",{
		.type=BPF_MAP_TYPE_PERCPU_ARRAY,.key_size=4,.value_size=8,.max_ents=4})==fd);
	std::vector<uint64_t> values(cpus);
	for (int i=0;i<cpus;++i) values[i]=1000+i;
	for (uint32_t key=0;key<4;++key)
		REQUIRE(bpftime_map_update_elem(fd,&key,values.data(),BPF_ANY)==0);
	auto prog=make_program(fd,true,(void *)bpftime_percpu_array_map_lookup_elem_helper);
	std::vector<uint64_t> addresses(cpus);
	for (int pass=0;pass<2;++pass) {
		for (int cpu=0;cpu<cpus;++cpu) {
			if (!CPU_ISSET(cpu,&original)) continue;
			cpu_set_t mask; CPU_ZERO(&mask); CPU_SET(cpu,&mask);
			REQUIRE(sched_setaffinity(0,sizeof(mask),&mask)==0);
			REQUIRE(sched_getcpu()==cpu);
			if (pass==0) addresses[cpu]=lookup(*prog,2);
			else REQUIRE(lookup(*prog,2)==addresses[cpu]);
			REQUIRE(*reinterpret_cast<uint64_t *>(addresses[cpu])==uint64_t(1000+cpu));
			REQUIRE(lookup(*prog,4,ENOENT)==0);
		}
		bpftime_close(fd);
		create_array(9999,8); // Same FD, different map type and capacity.
	}
	shm_holder.global_shared_memory.begin_new_session();
	REQUIRE(lookup(*prog,UINT32_MAX,ENOENT)==0);
	const auto child=fork();
	REQUIRE(child>=0);
	if (child==0) {
		uint32_t key=2; uint64_t result=0;
		int status=prog->bpftime_prog_exec(&key,4,&result);
		bool ok=status==0 && result!=0 &&
			*reinterpret_cast<uint64_t *>(result)==uint64_t(1000+sched_getcpu());
		prog.reset();
		_exit(ok ? 0:1);
	}
	int status=0; REQUIRE(waitpid(child,&status,0)==child);
	REQUIRE(WIFEXITED(status)); REQUIRE(WEXITSTATUS(status)==0);
	REQUIRE(*reinterpret_cast<uint64_t *>(lookup(*prog,2))==uint64_t(1000+sched_getcpu()));
	prog.reset();
	REQUIRE(memory->get_free_memory()==free_before);
}

TEST_CASE("Per-CPU array JIT does not introduce custom CPU helper calls",
	"[array-jit-lifetime][percpu-array-jit]")
{
	isolated_shm fixture;
	const int cpus=sysconf(_SC_NPROCESSORS_ONLN);
	REQUIRE(bpftime_maps_create(100,"percpu_custom_cpu",{
		.type=BPF_MAP_TYPE_PERCPU_ARRAY,.key_size=4,.value_size=8,.max_ents=4})==100);
	std::vector<uint64_t> values(cpus,42);
	uint32_t key=1;
	REQUIRE(bpftime_map_update_elem(100,&key,values.data(),BPF_ANY)==0);
	auto prog=make_program(100,true,(void *)bpftime_percpu_array_map_lookup_elem_helper,false,(void *)custom_lookup);
	REQUIRE(*reinterpret_cast<uint64_t *>(lookup(*prog,1))==42);
	bpftime_close(100);
	create_array(88);
	// Not bound: a replaced FD must not retain the per-CPU map.
	REQUIRE(*reinterpret_cast<uint64_t *>(lookup(*prog,1))==88);
}

uint64_t lookup(bpftime_prog &prog, uint32_t key, int expected_errno)
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

#if defined(__linux__)
TEST_CASE("Direct per-CPU JIT query preserves success and exact CPU failure behavior",
	"[array-jit-lifetime][percpu-array-jit]")
{
	isolated_shm fixture;
	const int cpus=sysconf(_SC_NPROCESSORS_ONLN);
	REQUIRE(cpus>0);
	REQUIRE(bpftime_maps_create(100,"percpu_cpu_query",{
		.type=BPF_MAP_TYPE_PERCPU_ARRAY,.key_size=4,.value_size=8,.max_ents=4})==100);
	std::vector<uint64_t> values(cpus);
	for (int i=0;i<cpus;++i) values[i]=2000+i;
	uint32_t key=1;
	REQUIRE(bpftime_map_update_elem(100,&key,values.data(),BPF_ANY)==0);
	auto prog=make_program(100,true,(void *)bpftime_percpu_array_map_lookup_elem_helper);
	std::ostringstream messages;
	auto old_logger=spdlog::default_logger();
	auto sink=std::make_shared<spdlog::sinks::ostream_sink_mt>(messages);
	spdlog::set_default_logger(std::make_shared<spdlog::logger>("cpu-query-test",sink));
	struct restore_query {
		std::shared_ptr<spdlog::logger> logger;
		~restore_query() { cpu_query_injected=false; spdlog::set_default_logger(logger); }
	} restore{old_logger};
	cpu_query_injected=true;
	for (int cpu : {0,cpus-1}) {
		cpu_query_result=cpu; cpu_query_reads=0;
		auto address=lookup(*prog,1);
		REQUIRE(cpu_query_reads==1);
		REQUIRE(*reinterpret_cast<uint64_t *>(address)==uint64_t(2000+cpu));
	}
	// Raw -1 must preserve the SAME log-once/CPU-0 behavior as helper 8.
	cpu_query_result=-1; cpu_query_reads=0; errno=EDOM;
	REQUIRE(bpftime_get_smp_processor_id()==0);
	REQUIRE(cpu_query_reads==1);
	const int expected_errno=errno;
	REQUIRE(messages.str().find("sched_getcpu error")!=std::string::npos);
	messages.str(""); messages.clear();
	cpu_query_reads=0; errno=EDOM;
	uint64_t result=0;
	REQUIRE(prog->bpftime_prog_exec(&key,4,&result)==0);
	REQUIRE(cpu_query_reads==1);
	REQUIRE(errno==expected_errno);
	REQUIRE(result!=0);
	REQUIRE(*reinterpret_cast<uint64_t *>(result)==2000);
	auto text=messages.str();
	auto pos=text.find("sched_getcpu error");
	REQUIRE(pos!=std::string::npos);
	REQUIRE(text.find("sched_getcpu error",pos+1)==std::string::npos);
	for (int invalid_cpu : {-2,cpus}) {
		cpu_query_result=invalid_cpu; cpu_query_reads=0;
		REQUIRE(lookup(*prog,1,ENOENT)==0);
		REQUIRE(cpu_query_reads==1);
	}
	cpu_query_result=-1; cpu_query_reads=0;
	REQUIRE(lookup(*prog,4,ENOENT)==0);
	REQUIRE(cpu_query_reads==0); // A proven miss must not query/log CPU.
	cpu_query_reads=0; result=99; errno=EDOM;
	REQUIRE(prog->bpftime_prog_exec(nullptr,0,&result)==0);
	REQUIRE(result==0);
	REQUIRE(errno==ENOENT);
	REQUIRE(cpu_query_reads==0);
}
#endif

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
		if (std::string(backend) == "llvm") {
			REQUIRE(prog.bpftime_prog_load(true) == 0);
			REQUIRE(*reinterpret_cast<uint64_t *>(lookup(prog,1)) == 31);
		}
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

TEST_CASE("Default array JIT preserves standard mutations on retained per-CPU arrays",
	"[array-jit-default][array-jit-lifetime]")
{
	isolated_shm fixture;
	cpu_set_t original,one;
	REQUIRE(sched_getaffinity(0,sizeof(original),&original)==0);
	struct affinity_restore {cpu_set_t mask; ~affinity_restore(){sched_setaffinity(0,sizeof(mask),&mask);}} restore{original};
	CPU_ZERO(&one); CPU_SET(sched_getcpu(),&one);
	REQUIRE(sched_setaffinity(0,sizeof(one),&one)==0);
	constexpr int fd=100;
	REQUIRE(bpftime_maps_create(fd,"percpu_mutations",{.type=BPF_MAP_TYPE_PERCPU_ARRAY,
		.key_size=4,.value_size=8,.max_ents=4})==fd);
	std::vector<uint64_t> values(sysconf(_SC_NPROCESSORS_ONLN),12);
	uint32_t key=1;
	REQUIRE(bpftime_map_update_elem(fd,&key,values.data(),BPF_ANY)==0);
	ebpf_inst code[]={
		{0xbf,6,1,0,0},
		{0x18,1,BPF_PSEUDO_MAP_FD,0,fd},{0,0,0,0,0},
		{0xbf,2,6,0,0},{0xbf,3,6,0,0},{0x07,3,0,0,8},{0xb7,4,0,0,BPF_ANY},
		{0x85,0,0,0,2},{0x7b,6,0,16,0},
		{0x18,1,BPF_PSEUDO_MAP_FD,0,fd},{0,0,0,0,0},
		{0xbf,2,6,0,0},{0x85,0,0,0,1},{0x79,7,0,0,0},
		{0x18,1,BPF_PSEUDO_MAP_FD,0,fd},{0,0,0,0,0},
		{0xbf,2,6,0,0},{0x85,0,0,0,3},{0x7b,6,0,24,0},
		{0xbf,0,7,0,0},{0x95,0,0,0,0},
	};
	bpftime_prog prog(code,sizeof(code)/sizeof(code[0]),"default_percpu_mutations");
	runtime_config config; config.set_vm_name("llvm"); config.jit_enabled=true;
	config.enable_shm_maps_helper_group=true; config.enable_kernel_helper_group=true;
	captured_array_log logs;
	REQUIRE(load_prog_and_helpers(&prog,config)==0);
	REQUIRE(logs.messages.str().find("inlined 1 lookup sites")!=std::string::npos);
	struct {uint32_t key,pad;uint64_t value,update_status,delete_status;} input{1,0,37,99,99};
	bpftime_close(fd); create_array(999,8);
	uint64_t result=0; errno=EDOM;
	REQUIRE(prog.bpftime_prog_exec(&input,sizeof(input),&result)==0);
	REQUIRE(result==37); REQUIRE(input.update_status==0);
	REQUIRE(input.delete_status==UINT64_MAX); REQUIRE(errno==EINVAL);
	REQUIRE(*static_cast<const uint64_t *>(bpftime_map_lookup_elem(fd,&key))==999);
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

TEST_CASE("Fork child executes and releases its own array binding reference",
	"[array-jit-lifetime]")
{
	isolated_shm fixture;
	auto memory = shm_holder.global_shared_memory.retain_mapping();
	(void)map_binding_mutex(*memory);
	auto before = memory->get_free_memory();
	int fd = create_array(64);
	auto prog = make_program(fd);
	bpftime_close(fd); // The program is now the last real shared-map owner.
	auto child_before = memory->get_free_memory();
	pid_t child = fork();
	REQUIRE(child >= 0);
	if (child == 0) {
		uint32_t key = 1;
		uint64_t result = 0;
		int status = prog->bpftime_prog_exec(&key,sizeof(key),&result);
		bool hit = status == 0 && result != 0 &&
			*reinterpret_cast<uint64_t *>(result) == 64;
		// The parent's reference keeps the map alive.
		prog.reset();
		bool unchanged = memory->get_free_memory() == child_before;
		_exit(hit && unchanged ? 0 : 1);
	}
	int status = -1;
	REQUIRE(waitpid(child,&status,0) == child);
	REQUIRE(WIFEXITED(status));
	REQUIRE(WEXITSTATUS(status) == 0);
	REQUIRE(*reinterpret_cast<uint64_t *>(lookup(*prog,1)) == 64);
	prog.reset(); // Balanced counts: the last owner frees the map.
	REQUIRE(memory->get_free_memory() == before);
}

TEST_CASE("Fork child keeps its binding after the parent releases the map",
	"[array-jit-lifetime]")
{
	isolated_shm fixture;
	auto memory = shm_holder.global_shared_memory.retain_mapping();
	(void)map_binding_mutex(*memory);
	auto before = memory->get_free_memory();
	int fd = create_array(65);
	auto prog = make_program(fd);
	bpftime_close(fd);
	int ready[2], go[2];
	REQUIRE(pipe(ready) == 0);
	REQUIRE(pipe(go) == 0);
	pid_t child = fork();
	REQUIRE(child >= 0);
	if (child == 0) {
		char byte = 0;
		bool ok = write(ready[1], &byte, 1) == 1 &&
			read(go[0], &byte, 1) == 1;
		// The parent no longer owns the map; the child's reference does.
		uint32_t key = 3;
		uint64_t result = 0;
		ok = ok && prog->bpftime_prog_exec(&key,sizeof(key),&result) == 0 &&
			result != 0 && *reinterpret_cast<uint64_t *>(result) == 65;
		prog.reset();
		ok = ok && memory->get_free_memory() == before;
		_exit(ok ? 0 : 1);
	}
	char byte = 0;
	REQUIRE(read(ready[0], &byte, 1) == 1);
	auto parent_before = memory->get_free_memory();
	prog.reset();
	REQUIRE(memory->get_free_memory() == parent_before);
	REQUIRE(write(go[1], &byte, 1) == 1);
	int status = -1;
	REQUIRE(waitpid(child,&status,0) == child);
	REQUIRE(WIFEXITED(status));
	REQUIRE(WEXITSTATUS(status) == 0);
	REQUIRE(memory->get_free_memory() == before);
	for (int pipe_fd : {ready[0], ready[1], go[0], go[1]})
		close(pipe_fd);
}

namespace {
std::vector<ebpf_inst> consuming_mixed_program()
{
	std::vector<ebpf_inst> code{{0xbf,6,1,0,0}, {0xb7,7,0,0,0}};
	for (int fd : {100,101,102}) {
		code.insert(code.end(), {
			{0x18,1,BPF_PSEUDO_MAP_FD,0,fd}, {0,0,0,0,0},
			{0xbf,2,6,0,0}, {0x85,0,0,0,1},
			{0x15,0,0,2,0}, {0x79,0,0,0,0}, {0x0f,7,0,0,0},
		});
	}
	code.insert(code.end(), {
		// Ordinary array update, followed by a consuming lookup.
		{0x18,1,BPF_PSEUDO_MAP_FD,0,100}, {0,0,0,0,0},
		{0xbf,2,6,0,0}, {0xbf,3,6,0,0}, {0x07,3,0,0,8},
		{0xb7,4,0,0,BPF_ANY}, {0x85,0,0,0,2}, {0x7b,6,0,16,0},
		{0x18,1,BPF_PSEUDO_MAP_FD,0,100}, {0,0,0,0,0},
		{0xbf,2,6,0,0}, {0x85,0,0,0,1},
		{0x15,0,0,2,0}, {0x79,0,0,0,0}, {0x0f,7,0,0,0},
		// A custom unrelated helper must execute, not disqualify lookup.
		{0x85,0,0,0,999}, {0x0f,7,0,0,0},
		{0xbf,0,7,0,0}, {0x95,0,0,0,0},
	});
	return code;
}
unsigned unrelated_calls;
uint64_t unrelated_helper(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)
{
	++unrelated_calls;
	return 9;
}
}

TEST_CASE("Default array JIT specializes mixed maps with update and unrelated helpers",
	"[array-jit-default][array-jit-lifetime]")
{
	isolated_shm fixture;
	// Neither environment opt-in nor request_array_map_inlining is used.
	create_array(11);
	REQUIRE(bpftime_maps_create(101,"mixed_percpu",{.type=BPF_MAP_TYPE_PERCPU_ARRAY,
		.key_size=4,.value_size=8,.max_ents=4})==101);
	REQUIRE(bpftime_maps_create(102,"mixed_hash",{.type=BPF_MAP_TYPE_HASH,
		.key_size=4,.value_size=8,.max_ents=4})==102);
	std::vector<uint64_t> values(sysconf(_SC_NPROCESSORS_ONLN));
	for (auto &value : values) value=100;
	uint32_t key=1; uint64_t hash=7;
	REQUIRE(bpftime_map_update_elem(101,&key,values.data(),BPF_ANY)==0);
	REQUIRE(bpftime_map_update_elem(102,&key,&hash,BPF_ANY)==0);
	auto code=consuming_mixed_program();
	bpftime_prog prog(code.data(),code.size(),"mixed_default");
	runtime_config config;
	config.set_vm_name("llvm"); config.jit_enabled=true;
	config.enable_shm_maps_helper_group=true; config.enable_kernel_helper_group=true;
	std::map<int,bpftime_helper_info> custom{{999,{999,"unrelated",(void *)unrelated_helper}}};
	captured_array_log logs;
	REQUIRE(load_prog_and_helpers(&prog,config,nullptr,nullptr,&custom)==0);
	REQUIRE(logs.messages.str().find("retained 2 maps")!=std::string::npos);
	REQUIRE(logs.messages.str().find("inlined 3 lookup sites")!=std::string::npos);
	struct {uint32_t key,pad; uint64_t value,update_status;} input{1,0,23,99};
	uint64_t result=0; unrelated_calls=0;
	REQUIRE(prog.bpftime_prog_exec(&input,sizeof(input),&result)==0);
	REQUIRE(input.update_status==0);
	REQUIRE(result==11+100+7+23+9);
	REQUIRE(unrelated_calls==1);
	// Both inlined lookup and non-inlined standard update use the old map.
	bpftime_close(100); create_array(999,8);
	input.value=31;
	REQUIRE(prog.bpftime_prog_exec(&input,sizeof(input),&result)==0);
	REQUIRE(input.update_status==0);
	REQUIRE(result==23+100+7+31+9);
	REQUIRE(unrelated_calls==2);
	REQUIRE(*static_cast<const uint64_t *>(bpftime_map_lookup_elem(100,&key))==999);
}

TEST_CASE("Default array JIT preserves custom lookup and CPU helpers before compilation",
	"[array-jit-default][array-jit-lifetime]")
{
	isolated_shm fixture;
	create_array(17);
	ebpf_inst code[]={{0xbf,2,1,0,0},{0x18,1,BPF_PSEUDO_MAP_FD,0,100},
		{0,0,0,0,0},{0x85,0,0,0,1},{0x95,0,0,0,0}};
	runtime_config config; config.set_vm_name("llvm"); config.jit_enabled=true;
	config.enable_shm_maps_helper_group=true; config.enable_kernel_helper_group=true;
	SECTION("custom lookup is not replaced") {
		bpftime_prog prog(code,5,"custom_lookup_default");
		std::map<int,bpftime_helper_info> custom{{1,{1,"custom",(void *)custom_lookup}}};
		captured_array_log logs;
		REQUIRE(load_prog_and_helpers(&prog,config,nullptr,nullptr,&custom)==0);
		REQUIRE(logs.messages.str().find("[array-jit-inline]")==std::string::npos);
		REQUIRE(lookup(prog,1)==12345);
	}
	SECTION("custom CPU does not disable ordinary array inline or get called implicitly") {
		bpftime_prog prog(code,5,"custom_cpu_default");
		std::map<int,bpftime_helper_info> custom{{8,{8,"custom_cpu",(void *)unrelated_helper}}};
		captured_array_log logs;
		REQUIRE(load_prog_and_helpers(&prog,config,nullptr,nullptr,&custom)==0);
		REQUIRE(logs.messages.str().find("inlined 1 lookup sites")!=std::string::npos);
		unrelated_calls=0;
		REQUIRE(*reinterpret_cast<uint64_t *>(lookup(prog,1))==17);
		REQUIRE(unrelated_calls==0);
	}
	SECTION("custom CPU leaves per-CPU calls on original path in a mixed program") {
		REQUIRE(bpftime_maps_create(101,"mixed_custom_cpu",{.type=BPF_MAP_TYPE_PERCPU_ARRAY,
			.key_size=4,.value_size=8,.max_ents=4})==101);
		REQUIRE(bpftime_maps_create(102,"mixed_custom_hash",{.type=BPF_MAP_TYPE_HASH,
			.key_size=4,.value_size=8,.max_ents=4})==102);
		uint32_t key=1; uint64_t hash=7;
		std::vector<uint64_t> values(sysconf(_SC_NPROCESSORS_ONLN),100);
		REQUIRE(bpftime_map_update_elem(101,&key,values.data(),BPF_ANY)==0);
		REQUIRE(bpftime_map_update_elem(102,&key,&hash,BPF_ANY)==0);
		auto mixed=consuming_mixed_program();
		bpftime_prog prog(mixed.data(),mixed.size(),"mixed_custom_cpu_default");
		std::map<int,bpftime_helper_info> custom{
			{8,{8,"custom_cpu",(void *)custom_lookup}},
			{999,{999,"unrelated",(void *)unrelated_helper}}};
		captured_array_log logs;
		REQUIRE(load_prog_and_helpers(&prog,config,nullptr,nullptr,&custom)==0);
		REQUIRE(logs.messages.str().find("retained 1 maps")!=std::string::npos);
		REQUIRE(logs.messages.str().find("inlined 2 lookup sites")!=std::string::npos);
		struct {uint32_t key,pad;uint64_t value,status;} input{1,0,23,99};
		uint64_t result=0;
		REQUIRE(prog.bpftime_prog_exec(&input,sizeof(input),&result)==0);
		REQUIRE(result==17+100+7+23+9); REQUIRE(input.status==0);
	}
}

TEST_CASE("Default array JIT keeps AOT on original helper registrations",
	"[array-jit-default][array-jit-lifetime]")
{
	isolated_shm fixture;
	create_array(71);
	ebpf_inst code[]={{0xbf,2,1,0,0},{0x18,1,BPF_PSEUDO_MAP_FD,0,100},
		{0,0,0,0,0},{0x85,0,0,0,1},{0x95,0,0,0,0}};
	auto *vm=ebpf_create("llvm");
	struct vm_owner {ebpf_vm *vm; ~vm_owner(){ebpf_destroy(vm);}} owner{vm};
	REQUIRE(ebpf_register(vm,1,"bpf_map_lookup_elem",(void *)bpftime_map_lookup_elem_helper)==0);
	ebpf_set_lddw_helpers(vm,map_ptr_by_fd,nullptr,map_val,nullptr,nullptr);
	char *error=nullptr;
	REQUIRE(ebpf_load(vm,code,sizeof(code),&error)==0);
	auto object=vm->vm_instance->do_aot_compile(false);
	REQUIRE(!object.empty());
	bpftime_prog prog(code,5,"default_aot");
	REQUIRE(prog.bpftime_prog_register_raw_helper({1,"bpf_map_lookup_elem",(void *)bpftime_map_lookup_elem_helper})==0);
	captured_array_log logs;
	REQUIRE(prog.load_aot_object(object)==0);
	REQUIRE(logs.messages.str().find("[array-jit-inline]")==std::string::npos);
	REQUIRE(*reinterpret_cast<uint64_t *>(lookup(prog,1))==71);
	bpftime_close(100); create_array(72);
	REQUIRE(*reinterpret_cast<uint64_t *>(lookup(prog,1))==72);
}

namespace {
// Reads global u64 at `offset`, stores the input's u64 into global slot 0,
// then adds the consumed array lookup value. Input: {u32 key, pad, u64 store}.
std::vector<ebpf_inst> global_data_program(int lookup_fd, int value_fd, int32_t offset = 8)
{
	return {
		{0xbf,6,1,0,0},
		{0x18,1,BPF_PSEUDO_MAP_VALUE,0,value_fd}, {0,0,0,0,offset},
		{0x79,7,1,0,0},
		{0x18,1,BPF_PSEUDO_MAP_VALUE,0,value_fd}, {0,0,0,0,0},
		{0x79,3,6,8,0}, {0x7b,1,3,0,0},
		{0x18,1,BPF_PSEUDO_MAP_FD,0,lookup_fd}, {0,0,0,0,0},
		{0xbf,2,6,0,0}, {0x85,0,0,0,BPF_FUNC_map_lookup_elem},
		{0x15,0,0,2,0}, {0x79,0,0,0,0}, {0x0f,0,7,0,0},
		{0x95,0,0,0,0},
	};
}
int create_global_data(int fd, uint64_t first, uint64_t second, uint32_t entries = 1)
{
	REQUIRE(bpftime_maps_create(fd, "prog.bss", {.type = BPF_MAP_TYPE_ARRAY,
		.key_size = 4, .value_size = 16, .max_ents = entries}) == fd);
	uint64_t value[2] = {first, second};
	const uint32_t key = 0;
	REQUIRE(bpftime_map_update_elem(fd, &key, value, BPF_ANY) == 0);
	return fd;
}
const uint64_t *global_slots(int fd)
{
	const uint32_t key = 0;
	return static_cast<const uint64_t *>(bpftime_map_lookup_elem(fd, &key));
}
std::unique_ptr<bpftime_prog> load_global_program(const std::vector<ebpf_inst> &code,
	void *helper = reinterpret_cast<void *>(bpftime_array_map_lookup_elem_helper), bool jit = true)
{
	auto prog = std::make_unique<bpftime_prog>(code.data(), code.size(), "array_jit_global");
	REQUIRE(prog->bpftime_prog_register_raw_helper({1, "bpf_map_lookup_elem", helper}) == 0);
	REQUIRE(prog->bpftime_prog_load(jit) == 0);
	return prog;
}
uint64_t run_global(bpftime_prog &prog, uint32_t key, uint64_t store)
{
	struct {uint32_t key, pad; uint64_t store;} input{key, 0, store};
	uint64_t result = 0;
	REQUIRE(prog.bpftime_prog_exec(&input, sizeof(input), &result) == 0);
	return result;
}
}

TEST_CASE("Array JIT owns global data of bound programs across close, reuse and reset",
	"[array-jit-lifetime][array-jit-global]")
{
	isolated_shm fixture;
	create_array(42);
	create_global_data(103, 5, 1000);
	captured_array_log logs;
	auto code = global_data_program(100, 103);
	auto prog = load_global_program(code);
	REQUIRE(logs.messages.str().find("retained 1 maps and 1 global data maps") != std::string::npos);
	REQUIRE(logs.messages.str().find("inlined 1 lookup sites") != std::string::npos);
	// The stored program keeps its relocations; only the loaded copy is rewritten.
	REQUIRE(prog->get_insns()[1].src_reg == BPF_PSEUDO_MAP_VALUE);
	REQUIRE(prog->get_insns()[2].imm == 8);
	REQUIRE(run_global(*prog, 2, 77) == 42 + 1000);
	REQUIRE(global_slots(103)[0] == 77); // Program writes reach the shared value.
	const uint32_t zero = 0;
	uint64_t updated[2] = {6, 2000};
	REQUIRE(bpftime_map_update_elem(103, &zero, updated, BPF_ANY) == 0);
	REQUIRE(run_global(*prog, 1, 78) == 42 + 2000);
	REQUIRE(run_global(*prog, 4, 79) == 0); // Proven miss: the program returns 0.
	REQUIRE(global_slots(103)[0] == 79);
	// A new map at the same FD must not become this program's global data.
	bpftime_close(103);
	create_global_data(103, 0, 9999);
	REQUIRE(run_global(*prog, 3, 80) == 42 + 2000);
	REQUIRE(global_slots(103)[0] == 0);
	REQUIRE(global_slots(103)[1] == 9999);
	shm_holder.global_shared_memory.begin_new_session();
	REQUIRE(run_global(*prog, 3, 81) == 42 + 2000);
	REQUIRE(prog->prepare_for_signal_execution() == -ENOTSUP);
	REQUIRE(prog->load_aot_object({}) == -ENOTSUP);
	REQUIRE(prog->bpftime_prog_load(true) == -EBUSY);
}

TEST_CASE("Array JIT shares a bound global data map with its lookup binding",
	"[array-jit-lifetime][array-jit-global]")
{
	isolated_shm fixture;
	auto memory = shm_holder.global_shared_memory.retain_mapping();
	(void)map_binding_mutex(*memory); // Exclude the persistent mutex allocation.
	auto before = memory->get_free_memory();
	// The same single-element array is both looked up and addressed directly.
	create_global_data(103, 9, 300);
	captured_array_log logs;
	auto prog = load_global_program(global_data_program(103, 103));
	REQUIRE(logs.messages.str().find("retained 1 maps and 1 global data maps") != std::string::npos);
	REQUIRE(run_global(*prog, 0, 50) == 50 + 300); // The store precedes the lookup.
	bpftime_close(103);
	REQUIRE(run_global(*prog, 0, 51) == 51 + 300);
	REQUIRE(run_global(*prog, 1, 52) == 0);
	prog.reset(); // The last owner releases the closed map exactly once.
	REQUIRE(memory->get_free_memory() == before);
}

TEST_CASE("Array JIT keeps unsupported global data programs on the original path",
	"[array-jit-lifetime][array-jit-global]")
{
	isolated_shm fixture;
	create_array(42);
	captured_array_log logs;
	SECTION("multi-element map") {
		create_global_data(103, 5, 1000, 4);
		auto prog = load_global_program(global_data_program(100, 103));
		REQUIRE(logs.messages.str().find("[array-jit-inline]") == std::string::npos);
		REQUIRE(run_global(*prog, 2, 1) == 42 + 1000);
	}
	SECTION("offset outside the value") {
		create_global_data(103, 5, 1000);
		auto prog = load_global_program(global_data_program(100, 103, 16));
		REQUIRE(logs.messages.str().find("[array-jit-inline]") == std::string::npos);
	}
	SECTION("hash map value") {
		REQUIRE(bpftime_maps_create(103, "not_global", {.type = BPF_MAP_TYPE_HASH,
			.key_size = 4, .value_size = 16, .max_ents = 1}) == 103);
		auto prog = load_global_program(global_data_program(100, 103));
		REQUIRE(logs.messages.str().find("[array-jit-inline]") == std::string::npos);
	}
	SECTION("per-CPU map value") {
		REQUIRE(bpftime_maps_create(103, "percpu_global", {.type = BPF_MAP_TYPE_PERCPU_ARRAY,
			.key_size = 4, .value_size = 16, .max_ents = 1}) == 103);
		auto prog = load_global_program(global_data_program(100, 103));
		REQUIRE(logs.messages.str().find("[array-jit-inline]") == std::string::npos);
	}
	SECTION("no array lookup to specialize") {
		create_global_data(103, 5, 1000);
		REQUIRE(bpftime_maps_create(102, "global_hash", {.type = BPF_MAP_TYPE_HASH,
			.key_size = 4, .value_size = 8, .max_ents = 4}) == 102);
		uint32_t key = 2;
		uint64_t value = 7;
		REQUIRE(bpftime_map_update_elem(102, &key, &value, BPF_ANY) == 0);
		auto prog = load_global_program(global_data_program(102, 103),
			reinterpret_cast<void *>(bpftime_map_lookup_elem_helper));
		REQUIRE(logs.messages.str().find("[array-jit-inline]") == std::string::npos);
		REQUIRE(run_global(*prog, 2, 3) == 7 + 1000);
		REQUIRE(global_slots(103)[0] == 3);
	}
	SECTION("other LDDW relocations") {
		create_global_data(103, 5, 1000);
		auto code = global_data_program(100, 103);
		code.insert(code.begin(), {{0x18,0,BPF_PSEUDO_BTF_ID,0,1}, {0,0,0,0,0}});
		bpftime_prog prog(code.data(), code.size(), "array_jit_btf_id");
		REQUIRE(prog.bpftime_prog_register_raw_helper({1, "bpf_map_lookup_elem",
			reinterpret_cast<void *>(bpftime_array_map_lookup_elem_helper)}) == 0);
		(void)prog.bpftime_prog_load(true); // Backend support for BTF IDs is not under test.
		REQUIRE(logs.messages.str().find("[array-jit-inline]") == std::string::npos);
	}
	SECTION("uBPF backend") {
		// Never bound; execution is not asserted because the uBPF
		// interpreter's own bounds check rejects global data addresses.
		create_global_data(103, 5, 1000);
		runtime_config config;
		config.set_vm_name("ubpf");
		shm_holder.global_shared_memory.set_runtime_config(std::move(config));
		auto prog = load_global_program(global_data_program(100, 103),
			reinterpret_cast<void *>(bpftime_array_map_lookup_elem_helper), false);
		REQUIRE(logs.messages.str().find("[array-jit-inline]") == std::string::npos);
		REQUIRE(prog->get_insns()[1].src_reg == BPF_PSEUDO_MAP_VALUE);
	}
}

TEST_CASE("Bound programs dispatch non-array and dynamic handles to their original targets",
	"[array-jit-lifetime][array-jit-dispatch]")
{
	isolated_shm fixture;
	create_array(40); // fd 100: bound and inlined
	REQUIRE(bpftime_maps_create(105, "dispatch_hash", {.type = BPF_MAP_TYPE_HASH,
		.key_size = 4, .value_size = 8, .max_ents = 4}) == 105);
	REQUIRE(bpftime_maps_create(102, "dispatch_dynamic", {.type = BPF_MAP_TYPE_ARRAY,
		.key_size = 4, .value_size = 8, .max_ents = 4}) == 102);
	uint32_t one = 1;
	uint64_t two = 2;
	REQUIRE(bpftime_map_update_elem(102, &one, &two, BPF_ANY) == 0);
	// hash update; array lookup; hash lookup; dynamic lookup; hash delete.
	std::vector<ebpf_inst> code{
		{0xbf,6,1,0,0},
		{0x18,1,BPF_PSEUDO_MAP_FD,0,105}, {0,0,0,0,0}, {0xbf,2,6,0,0}, {0xbf,3,6,0,0},
		{0x07,3,0,0,16}, {0xb7,4,0,0,BPF_ANY}, {0x85,0,0,0,BPF_FUNC_map_update_elem},
		{0x7b,6,0,24,0}, {0xb7,7,0,0,0},
		{0x18,1,BPF_PSEUDO_MAP_FD,0,100}, {0,0,0,0,0}, {0xbf,2,6,0,0},
		{0x85,0,0,0,BPF_FUNC_map_lookup_elem}, {0x15,0,0,1,0}, {0x79,7,0,0,0},
		{0x18,1,BPF_PSEUDO_MAP_FD,0,105}, {0,0,0,0,0}, {0xbf,2,6,0,0},
		{0x85,0,0,0,BPF_FUNC_map_lookup_elem}, {0x15,0,0,2,0}, {0x79,0,0,0,0}, {0x0f,7,0,0,0},
		{0x79,1,6,8,0}, {0xbf,2,6,0,0},
		{0x85,0,0,0,BPF_FUNC_map_lookup_elem}, {0x15,0,0,2,0}, {0x79,0,0,0,0}, {0x0f,7,0,0,0},
		{0x18,1,BPF_PSEUDO_MAP_FD,0,105}, {0,0,0,0,0}, {0xbf,2,6,0,0},
		{0x85,0,0,0,BPF_FUNC_map_delete_elem}, {0x7b,6,0,32,0},
		{0xbf,0,7,0,0}, {0x95,0,0,0,0},
	};
	bpftime_prog prog(code.data(), code.size(), "dispatch_default");
	runtime_config config;
	config.set_vm_name("llvm"); config.jit_enabled = true;
	config.enable_shm_maps_helper_group = true; config.enable_kernel_helper_group = true;
	captured_array_log logs;
	REQUIRE(load_prog_and_helpers(&prog, config, nullptr, nullptr, nullptr) == 0);
	REQUIRE(logs.messages.str().find("retained 1 maps") != std::string::npos);
	REQUIRE(logs.messages.str().find("inlined 1 lookup sites") != std::string::npos);
	// Constant hash handle: update, lookup and delete call the original
	// helpers directly; the dynamic lookup keeps the dispatcher.
	REQUIRE(logs.messages.str().find("3 map helper calls use the original helper directly") != std::string::npos);
	struct {uint32_t key, pad; uint64_t dynamic_fd, value, update_status, delete_status;} input{};
	auto run = [&](uint64_t dynamic_fd) {
		input = {1, 0, dynamic_fd, 500, 99, 99};
		uint64_t result = 0;
		REQUIRE(prog.bpftime_prog_exec(&input, sizeof(input), &result) == 0);
		REQUIRE(input.update_status == 0);
		REQUIRE(input.delete_status == 0);
		return result;
	};
	REQUIRE(run(102) == 40 + 500 + 2);   // Dynamic FD inside the table range.
	REQUIRE(bpftime_map_lookup_elem(105, &one) == nullptr); // Deleted again.
	REQUIRE(run(100) == 40 + 500 + 40);  // Dynamic use of the bound FD.
	REQUIRE(run(105) == 40 + 500 + 500); // Dynamic use of the unbound hash.
	// Non-array maps are not pinned: a replacement at the same FD is used.
	bpftime_close(105);
	REQUIRE(bpftime_maps_create(105, "dispatch_hash_2", {.type = BPF_MAP_TYPE_HASH,
		.key_size = 4, .value_size = 8, .max_ents = 4}) == 105);
	uint32_t key2 = 2;
	uint64_t kept = 77;
	REQUIRE(bpftime_map_update_elem(105, &key2, &kept, BPF_ANY) == 0);
	REQUIRE(run(102) == 40 + 500 + 2);
	REQUIRE(*static_cast<const uint64_t *>(bpftime_map_lookup_elem(105, &key2)) == 77);
	REQUIRE(bpftime_map_lookup_elem(105, &one) == nullptr);
}

namespace {
// Stores the input value into hash[key], then sums the other map's value,
// hash[key] and, with value_fd, the global u64 at offset 8.
// Input: {u32 key, pad, u64 value}. A negative other_fd skips that lookup.
std::vector<ebpf_inst> typed_selection_program(int hash_fd, int other_fd, int value_fd = -1)
{
	std::vector<ebpf_inst> code{
		{0xbf,6,1,0,0}, {0xb7,7,0,0,0},
		{0x18,1,BPF_PSEUDO_MAP_FD,0,hash_fd}, {0,0,0,0,0}, {0xbf,2,6,0,0}, {0xbf,3,6,0,0},
		{0x07,3,0,0,8}, {0xb7,4,0,0,BPF_ANY}, {0x85,0,0,0,BPF_FUNC_map_update_elem},
		{0x18,1,BPF_PSEUDO_MAP_FD,0,hash_fd}, {0,0,0,0,0}, {0xbf,2,6,0,0},
		{0x85,0,0,0,BPF_FUNC_map_lookup_elem}, {0x15,0,0,2,0}, {0x79,0,0,0,0}, {0x0f,7,0,0,0},
		{0xbf,0,7,0,0}, {0x95,0,0,0,0},
	};
	if (other_fd >= 0)
		code.insert(code.begin() + 9, {{0x18,1,BPF_PSEUDO_MAP_FD,0,other_fd}, {0,0,0,0,0},
			{0xbf,2,6,0,0}, {0x85,0,0,0,BPF_FUNC_map_lookup_elem}, {0x15,0,0,2,0},
			{0x79,0,0,0,0}, {0x0f,7,0,0,0}});
	if (value_fd >= 0)
		code.insert(code.end() - 2, {{0x18,1,BPF_PSEUDO_MAP_VALUE,0,value_fd}, {0,0,0,0,8},
			{0x79,1,1,0,0}, {0x0f,7,1,0,0}});
	return code;
}
struct typed_selection {
	captured_array_log logs;
	bpf_attach_ctx ctx;
	bpftime_prog *prog = nullptr;
	typed_selection(const std::vector<ebpf_inst> &code, const char *name)
	{
		constexpr int prog_fd = 120;
		REQUIRE(bpftime_progs_create(prog_fd, code.data(), code.size(), name,
			(int)bpftime::bpf_prog_type::BPF_PROG_TYPE_KPROBE) == prog_fd);
		runtime_config config;
		config.set_vm_name("llvm"); config.jit_enabled = true;
		config.enable_shm_maps_helper_group = true; config.enable_kernel_helper_group = true;
		REQUIRE(ctx.init_attach_ctx_from_handlers(config) == 0);
		prog = ctx.find_instantiated_prog(prog_fd);
		REQUIRE(prog != nullptr);
	}
	bool logged(const std::string &text) const
	{
		return logs.messages.str().find(text) != std::string::npos;
	}
	uint64_t run(uint32_t key, uint64_t value)
	{
		struct {uint32_t key, pad; uint64_t value;} input{key, 0, value};
		uint64_t result = 0;
		REQUIRE(prog->bpftime_prog_exec(&input, sizeof(input), &result) == 0);
		return result;
	}
};
void create_hash(int fd, int type = BPF_MAP_TYPE_HASH)
{
	REQUIRE(bpftime_maps_create(fd, "typed_selection_hash", {.type = type,
		.key_size = 4, .value_size = 8, .max_ents = 4}) == fd);
}
}

TEST_CASE("Typed hash helpers serve hash maps next to arrays and global data",
	"[array-jit-lifetime][typed-helper-selection]")
{
	isolated_shm fixture;
	create_hash(105);
	SECTION("hash only") {
		typed_selection selected(typed_selection_program(105, -1), "typed_hash_only");
		REQUIRE(selected.logged("Using specialized hash lookup helper for program typed_hash_only"));
		REQUIRE(selected.logged("Using specialized hash update helper for program typed_hash_only"));
		REQUIRE(selected.run(1, 500) == 500);
	}
	SECTION("hash with global data") {
		create_global_data(103, 5, 1000);
		typed_selection selected(typed_selection_program(105, -1, 103), "typed_hash_global");
		REQUIRE(selected.logged("Using specialized hash lookup helper for program typed_hash_global"));
		REQUIRE(selected.logged("Using specialized hash update helper for program typed_hash_global"));
		REQUIRE(selected.run(1, 500) == 500 + 1000);
	}
	SECTION("hash with inlined array, global data and a tail call map") {
		create_array(40);
		create_global_data(103, 5, 1000);
		REQUIRE(bpftime_maps_create(107, "typed_selection_progs", {
			.type = BPF_MAP_TYPE_PROG_ARRAY, .key_size = 4, .value_size = 4,
			.max_ents = 4}) == 107);
		auto code = typed_selection_program(105, 100, 103);
		// The tail call map is loaded but never passed to a map helper.
		code.insert(code.begin(), {{0x18,0,BPF_PSEUDO_MAP_FD,0,107}, {0,0,0,0,0}});
		typed_selection selected(code, "typed_hash_mixed");
		REQUIRE(selected.logged("Using specialized hash lookup helper for program typed_hash_mixed"));
		REQUIRE(selected.logged("Using specialized hash update helper for program typed_hash_mixed"));
		REQUIRE(selected.logged("inlined 1 lookup sites"));
		REQUIRE(selected.run(1, 500) == 40 + 500 + 1000);
		REQUIRE(selected.run(9, 7) == 7 + 1000); // Array miss, hash insert.
		uint32_t key = 1;
		REQUIRE(*static_cast<const uint64_t *>(bpftime_map_lookup_elem(105, &key)) == 500);
	}
	SECTION("two hash kinds stay generic") {
		create_hash(106, BPF_MAP_TYPE_PERCPU_HASH);
		typed_selection selected(typed_selection_program(105, 106), "typed_two_hashes");
		REQUIRE_FALSE(selected.logged("Using specialized"));
		REQUIRE(selected.run(1, 500) == 500);
	}
	SECTION("other lookup map kinds stay generic") {
		create_hash(108, BPF_MAP_TYPE_LRU_HASH);
		typed_selection selected(typed_selection_program(105, 108), "typed_lru");
		REQUIRE_FALSE(selected.logged("Using specialized"));
		REQUIRE(selected.run(1, 500) == 500);
	}
}

TEST_CASE("Typed hash helpers match the generic helper for other handles",
	"[array-jit-lifetime][typed-helper-selection]")
{
	isolated_shm fixture;
	create_array(40);
	uint32_t key = 1, missing = 4;
	uint64_t value = 6;
	auto key_arg = reinterpret_cast<uint64_t>(&key);
	auto value_arg = reinterpret_cast<uint64_t>(&value);
	REQUIRE(bpftime_hash_map_lookup_elem_helper(100, key_arg, 0, 0, 0) ==
		bpftime_map_lookup_elem_helper(100, key_arg, 0, 0, 0));
	REQUIRE(bpftime_hash_map_update_elem_helper(100, key_arg, value_arg, BPF_ANY, 0) == 0);
	REQUIRE(*static_cast<const uint64_t *>(bpftime_map_lookup_elem(100, &key)) == 6);
	errno = 0;
	REQUIRE(bpftime_hash_map_lookup_elem_helper(100, reinterpret_cast<uint64_t>(&missing), 0, 0, 0) == 0);
	const int array_miss_errno = errno;
	errno = 0;
	REQUIRE(bpftime_map_lookup_elem_helper(100, reinterpret_cast<uint64_t>(&missing), 0, 0, 0) == 0);
	REQUIRE(errno == array_miss_errno);
	for (uint64_t fd : {uint64_t(99), uint64_t(1u << 30)}) {
		errno = 0;
		REQUIRE(bpftime_hash_map_lookup_elem_helper(fd, key_arg, 0, 0, 0) == 0);
		REQUIRE(errno == ENOENT);
		errno = 0;
		REQUIRE(bpftime_map_lookup_elem_helper(fd, key_arg, 0, 0, 0) == 0);
		REQUIRE(errno == ENOENT);
		REQUIRE(bpftime_hash_map_update_elem_helper(fd, key_arg, value_arg, BPF_ANY, 0) ==
			static_cast<uint64_t>(-1));
		REQUIRE(bpftime_map_update_elem_helper(fd, key_arg, value_arg, BPF_ANY, 0) ==
			static_cast<uint64_t>(-1));
	}
}

namespace {
// Stores the input value into hash[key], deletes it again, records the delete
// status in input+16, and returns the lookup result after the delete (0 when
// gone). With array_fd, the program first looks up array[key] so the array
// JIT binds it. Input: {u32 key, pad, u64 value, u64 status}.
std::vector<ebpf_inst> typed_delete_program(int hash_fd, int array_fd = -1)
{
	std::vector<ebpf_inst> code{
		{0xbf,6,1,0,0},
		{0x18,1,BPF_PSEUDO_MAP_FD,0,hash_fd}, {0,0,0,0,0}, {0xbf,2,6,0,0}, {0xbf,3,6,0,0},
		{0x07,3,0,0,8}, {0xb7,4,0,0,BPF_ANY}, {0x85,0,0,0,BPF_FUNC_map_update_elem},
		{0x18,1,BPF_PSEUDO_MAP_FD,0,hash_fd}, {0,0,0,0,0}, {0xbf,2,6,0,0},
		{0x85,0,0,0,BPF_FUNC_map_delete_elem}, {0x7b,6,0,16,0},
		{0x18,1,BPF_PSEUDO_MAP_FD,0,hash_fd}, {0,0,0,0,0}, {0xbf,2,6,0,0},
		{0x85,0,0,0,BPF_FUNC_map_lookup_elem}, {0x95,0,0,0,0},
	};
	if (array_fd >= 0)
		code.insert(code.begin() + 1, {{0x18,1,BPF_PSEUDO_MAP_FD,0,array_fd}, {0,0,0,0,0},
			{0xbf,2,6,0,0}, {0x85,0,0,0,BPF_FUNC_map_lookup_elem}, {0x15,0,0,1,0},
			{0x79,0,0,0,0}});
	return code;
}
}

TEST_CASE("Typed hash delete helper serves hash maps in plain and bound programs",
	"[array-jit-lifetime][typed-helper-selection]")
{
	isolated_shm fixture;
	create_hash(105);
	struct {uint32_t key, pad; uint64_t value, status;} input{};
	auto check = [&](typed_selection &selected, const char *name) {
		REQUIRE(selected.logged(std::string("Using specialized hash delete helper for program ") + name));
		for (uint32_t key : {1u, 2u, 3u}) {
			input = {key, 0, 100 + key, 99};
			uint64_t result = 99;
			REQUIRE(selected.prog->bpftime_prog_exec(&input, sizeof(input), &result) == 0);
			REQUIRE(result == 0); // Deleted again.
			REQUIRE(input.status == 0);
			REQUIRE(bpftime_map_lookup_elem(105, &key) == nullptr);
		}
	};
	SECTION("hash only") {
		typed_selection selected(typed_delete_program(105), "typed_delete_hash");
		check(selected, "typed_delete_hash");
	}
	SECTION("hash next to an inlined array") {
		create_array(40);
		typed_selection selected(typed_delete_program(105, 100), "typed_delete_bound");
		REQUIRE(selected.logged("[array-jit-inline] program=typed_delete_bound retained 1 maps"));
		check(selected, "typed_delete_bound");
	}
}

TEST_CASE("Typed hash delete helper matches the generic helper for other handles",
	"[array-jit-lifetime][typed-helper-selection]")
{
	isolated_shm fixture;
	create_array(40);
	create_hash(105);
	uint32_t key = 1;
	uint64_t value = 7;
	auto key_arg = reinterpret_cast<uint64_t>(&key);
	// Array delete is unsupported; both helpers report the same result.
	errno = 0;
	const uint64_t typed = bpftime_hash_map_delete_elem_helper(100, key_arg, 0, 0, 0);
	const int typed_errno = errno;
	errno = 0;
	REQUIRE(bpftime_map_delete_elem_helper(100, key_arg, 0, 0, 0) == typed);
	REQUIRE(errno == typed_errno);
	REQUIRE(bpftime_map_update_elem(105, &key, &value, BPF_ANY) == 0);
	REQUIRE(bpftime_hash_map_delete_elem_helper(105, key_arg, 0, 0, 0) == 0);
	REQUIRE(bpftime_map_lookup_elem(105, &key) == nullptr);
	for (uint64_t fd : {uint64_t(99), uint64_t(1u << 30)}) {
		errno = 0;
		REQUIRE(bpftime_hash_map_delete_elem_helper(fd, key_arg, 0, 0, 0) == static_cast<uint64_t>(-1));
		REQUIRE(errno == ENOENT);
		errno = 0;
		REQUIRE(bpftime_map_delete_elem_helper(fd, key_arg, 0, 0, 0) == static_cast<uint64_t>(-1));
		REQUIRE(errno == ENOENT);
	}
}

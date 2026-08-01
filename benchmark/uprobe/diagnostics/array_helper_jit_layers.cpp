// SPDX-License-Identifier: MIT
// Diagnostic-only harness for separating LLVM JIT/helper-call overhead from
// bpftime's userspace array-map helper implementation.

#include "bpftime_shm.hpp"
#include "ebpf-vm.h"

#include <bpf/libbpf.h>
#include <linux/bpf.h>

#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <sched.h>

extern "C" {
uint64_t bpftime_map_lookup_elem_helper(uint64_t map, uint64_t key,
					uint64_t, uint64_t, uint64_t);
uint64_t bpftime_map_update_elem_helper(uint64_t map, uint64_t key,
					uint64_t value, uint64_t flags,
					uint64_t);
}

namespace
{

constexpr int kMapFd = 3;
constexpr unsigned kHelperId = 1000;
constexpr uint64_t kDefaultInvocations = 100000;
constexpr unsigned kDefaultRounds = 5;

#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

extern "C" NOINLINE uint64_t diagnostic_noop_helper(uint64_t, uint64_t,
						     uint64_t, uint64_t,
						     uint64_t)
{
	asm volatile("" ::: "memory");
	return 0;
}

struct options {
	std::string implementation;
	std::string operation;
	uint64_t invocations = kDefaultInvocations;
	unsigned rounds = kDefaultRounds;
	std::optional<int> cpu;
	std::string object = ARRAY_HELPER_JIT_BPF_OBJECT;
};

[[noreturn]] void usage(const char *program, const char *error = nullptr)
{
	if (error)
		std::fprintf(stderr, "error: %s\n", error);
	std::fprintf(stderr,
		     "usage: %s <noop|array> <lookup|update> [invocations] "
		     "[rounds] [cpu] [bpf-object]\n",
		     program);
	std::exit(2);
}

uint64_t parse_u64(const char *text, const char *field, bool allow_zero = false)
{
	char *end = nullptr;
	errno = 0;
	auto result = std::strtoull(text, &end, 10);
	if (errno || !end || *end != '\0' || (!allow_zero && result == 0))
		usage("array-helper-jit-layers", field);
	return result;
}

options parse_options(int argc, char **argv)
{
	if (argc < 3 || argc > 7)
		usage(argv[0]);
	options result{ .implementation = argv[1], .operation = argv[2] };
	if (result.implementation != "noop" &&
	    result.implementation != "array")
		usage(argv[0], "unknown helper implementation");
	if (result.operation != "lookup" && result.operation != "update")
		usage(argv[0], "unknown operation");
	if (argc >= 4)
		result.invocations = parse_u64(argv[3], "invalid invocation count");
	if (argc >= 5)
		result.rounds = static_cast<unsigned>(
			parse_u64(argv[4], "invalid round count"));
	if (argc >= 6)
		result.cpu = static_cast<int>(
			parse_u64(argv[5], "invalid CPU", true));
	if (argc >= 7)
		result.object = argv[6];
	return result;
}

void pin_to_cpu(int cpu)
{
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(cpu, &set);
	if (sched_setaffinity(0, sizeof(set), &set) != 0) {
		std::fprintf(stderr, "sched_setaffinity(cpu=%d): %s\n", cpu,
			     std::strerror(errno));
		std::exit(1);
	}
}

struct vm_holder {
	ebpf_vm *vm = nullptr;
	vm_holder() = default;
	explicit vm_holder(ebpf_vm *value) : vm(value) {}
	vm_holder(const vm_holder &) = delete;
	vm_holder &operator=(const vm_holder &) = delete;
	vm_holder(vm_holder &&other) noexcept : vm(other.vm)
	{
		other.vm = nullptr;
	}
	vm_holder &operator=(vm_holder &&other) noexcept
	{
		if (this != &other) {
			if (vm)
				ebpf_destroy(vm);
			vm = other.vm;
			other.vm = nullptr;
		}
		return *this;
	}
	~vm_holder()
	{
		if (vm)
			ebpf_destroy(vm);
	}
};

vm_holder create_compiled_vm(const options &opt, ebpf_jit_fn &function)
{
	bpf_object_open_opts open_options{};
	open_options.sz = sizeof(open_options);
	bpf_object *object = bpf_object__open_file(opt.object.c_str(),
						   &open_options);
	if (!object)
		throw std::runtime_error("failed to open diagnostic BPF object");

	const char *program_name =
		opt.operation == "lookup" ? "jit_lookup" : "jit_update";
	bpf_program *program = bpf_object__find_program_by_name(object,
							program_name);
	if (!program) {
		bpf_object__close(object);
		throw std::runtime_error("failed to find diagnostic BPF program");
	}

	vm_holder result(ebpf_create("llvm"));
	if (!result.vm) {
		bpf_object__close(object);
		throw std::runtime_error("failed to create LLVM VM");
	}
	void *helper = opt.implementation == "noop" ?
			       reinterpret_cast<void *>(diagnostic_noop_helper) :
			       (opt.operation == "lookup" ?
					reinterpret_cast<void *>(
						bpftime_map_lookup_elem_helper) :
					reinterpret_cast<void *>(
						bpftime_map_update_elem_helper));
	if (ebpf_register(result.vm, kHelperId, "diagnostic_helper", helper) !=
	    0) {
		bpf_object__close(object);
		throw std::runtime_error("failed to register diagnostic helper");
	}
	char *error = nullptr;
	if (ebpf_load(result.vm, bpf_program__insns(program),
		      static_cast<uint32_t>(bpf_program__insn_cnt(program) * 8),
		      &error) != 0) {
		const std::string message = error ? error : "unknown VM load error";
		std::free(error);
		bpf_object__close(object);
		throw std::runtime_error(message);
	}
	function = ebpf_compile(result.vm, &error);
	bpf_object__close(object);
	if (!function) {
		const std::string message = error ? error : "unknown JIT error";
		std::free(error);
		throw std::runtime_error(message);
	}
	return result;
}

NOINLINE uint64_t run(ebpf_jit_fn function, uint64_t invocations)
{
	uint64_t sink = 0;
	uint64_t context = 1;
	for (uint64_t i = 0; i < invocations; ++i)
		sink += function(&context, sizeof(context));
	return sink;
}

} // namespace

int main(int argc, char **argv)
{
	const options opt = parse_options(argc, argv);
	if (opt.cpu)
		pin_to_cpu(*opt.cpu);

	const std::string shm_name =
		"bpftime_array_helper_jit_" + std::to_string(getpid());
	if (setenv("BPFTIME_GLOBAL_SHM_NAME", shm_name.c_str(), 1) != 0) {
		std::perror("setenv");
		return 1;
	}

	try {
		bpftime_remove_global_shm();
		bpftime_initialize_global_shm(
			bpftime::shm_open_type::SHM_REMOVE_AND_CREATE);
		const bpftime::bpf_map_attr attr{
			.type = static_cast<int>(bpftime::bpf_map_type::BPF_MAP_TYPE_ARRAY),
			.key_size = sizeof(uint32_t),
			.value_size = sizeof(uint64_t),
			.max_ents = 1024,
		};
		if (bpftime_maps_create(kMapFd, "array_helper_jit", attr) !=
		    kMapFd)
			throw std::runtime_error("failed to create diagnostic array map");

		ebpf_jit_fn function = nullptr;
		auto vm = create_compiled_vm(opt, function);
		volatile uint64_t sink = run(function, 1000);
		std::printf("implementation=%s operation=%s invocations=%" PRIu64
			    " helpers_per_invocation=1000 rounds=%u cpu=%d\n",
			    opt.implementation.c_str(), opt.operation.c_str(),
			    opt.invocations, opt.rounds, sched_getcpu());
		for (unsigned round = 1; round <= opt.rounds; ++round) {
			const auto begin = std::chrono::steady_clock::now();
			sink += run(function, opt.invocations);
			const auto end = std::chrono::steady_clock::now();
			const uint64_t elapsed_ns =
				std::chrono::duration_cast<std::chrono::nanoseconds>(
					end - begin)
					.count();
			const double helpers =
				static_cast<double>(opt.invocations) * 1000.0;
			std::printf("round=%u elapsed_ns=%" PRIu64
				    " ns_per_helper=%.6f sink=%" PRIu64 "\n",
				    round, elapsed_ns,
				    static_cast<double>(elapsed_ns) / helpers,
				    static_cast<uint64_t>(sink));
		}

		bpftime_destroy_global_shm();
		bpftime_remove_global_shm();
	} catch (const std::exception &error) {
		std::fprintf(stderr, "fatal: %s\n", error.what());
		bpftime_destroy_global_shm();
		bpftime_remove_global_shm();
		return 1;
	}
	return 0;
}

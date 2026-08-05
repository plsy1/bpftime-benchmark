// SPDX-License-Identifier: MIT
// Matched LLVM-JIT noop/real helper diagnostic for ordinary and per-CPU maps.
// The BPF bytecode is identical; only the registered helper implementation and
// map type change.  This target does not modify the production runtime.

#include "bpftime_shm.hpp"
#include "bpftime_shm_internal.hpp"
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
uint64_t bpftime_map_lookup_elem_helper(uint64_t, uint64_t, uint64_t,
					uint64_t, uint64_t);
uint64_t bpftime_map_update_elem_helper(uint64_t, uint64_t, uint64_t,
					uint64_t, uint64_t);
}

namespace {

constexpr int kMapFd = 3;
constexpr unsigned kHelperId = 1000;
constexpr uint32_t kActiveKeys = 1000;
constexpr uint64_t kDefaultInvocations = 100000;
constexpr unsigned kDefaultRounds = 5;

#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

extern "C" NOINLINE uint64_t noop_helper(uint64_t, uint64_t, uint64_t,
						 uint64_t, uint64_t)
{
	asm volatile("" ::: "memory");
	return 0;
}

enum class map_kind { array, percpu_array, hash, percpu_hash };

struct options {
	map_kind kind;
	std::string operation;
	std::string mode;
	uint64_t invocations = kDefaultInvocations;
	unsigned rounds = kDefaultRounds;
	int cpu = -1;
};

[[noreturn]] void usage(const char *program, const char *error = nullptr)
{
	if (error)
		std::fprintf(stderr, "error: %s\n", error);
	std::fprintf(stderr,
		     "usage: %s <array|percpu_array|hash|percpu_hash> "
		     "<lookup|update> <pair|noop|real> [invocations] [rounds] [cpu]\n",
		     program);
	std::exit(2);
}

uint64_t parse_u64(const char *text, const char *field, bool allow_zero = false)
{
	char *end = nullptr;
	errno = 0;
	uint64_t value = std::strtoull(text, &end, 10);
	if (errno || !end || *end != '\0' || (!allow_zero && value == 0))
		usage("per-cpu-helper-jit-layers", field);
	return value;
}

map_kind parse_kind(const char *text)
{
	if (!std::strcmp(text, "array")) return map_kind::array;
	if (!std::strcmp(text, "percpu_array")) return map_kind::percpu_array;
	if (!std::strcmp(text, "hash")) return map_kind::hash;
	if (!std::strcmp(text, "percpu_hash")) return map_kind::percpu_hash;
	usage("per-cpu-helper-jit-layers", "unknown map kind");
}

options parse_options(int argc, char **argv)
{
	if (argc < 4 || argc > 7) usage(argv[0]);
	options result{parse_kind(argv[1]), argv[2], argv[3]};
	if (result.operation != "lookup" && result.operation != "update")
		usage(argv[0], "operation must be lookup or update");
	if (result.mode != "pair" && result.mode != "noop" && result.mode != "real")
		usage(argv[0], "mode must be pair, noop, or real");
	if (argc >= 5) result.invocations = parse_u64(argv[4], "invocations");
	if (argc >= 6) result.rounds = (unsigned)parse_u64(argv[5], "rounds");
	if (argc >= 7) result.cpu = (int)parse_u64(argv[6], "CPU", true);
	return result;
}

void pin_to_cpu(int cpu)
{
	if (cpu < 0) return;
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
	~vm_holder() { if (vm) ebpf_destroy(vm); }
};

void *real_helper(const options &opt)
{
	return opt.operation == "lookup"
		? reinterpret_cast<void *>(bpftime_map_lookup_elem_helper)
		: reinterpret_cast<void *>(bpftime_map_update_elem_helper);
}

vm_holder create_vm(const options &opt, bool real, ebpf_jit_fn &function)
{
	bpf_object_open_opts open_options{};
	open_options.sz = sizeof(open_options);
	bpf_object *object = bpf_object__open_file(ARRAY_HELPER_JIT_BPF_OBJECT,
							   &open_options);
	if (!object) throw std::runtime_error("failed to open BPF object");
	std::string program_name = "jit_" + opt.operation;
	bpf_program *program = bpf_object__find_program_by_name(
		object, program_name.c_str());
	if (!program) {
		bpf_object__close(object);
		throw std::runtime_error("failed to find JIT program");
	}
	vm_holder result(ebpf_create("llvm"));
	if (!result.vm) throw std::runtime_error("failed to create LLVM VM");
	void *helper = real ? real_helper(opt) : reinterpret_cast<void *>(noop_helper);
	if (ebpf_register(result.vm, kHelperId, "diagnostic_helper", helper) != 0)
		throw std::runtime_error("failed to register helper");
	char *error = nullptr;
	if (ebpf_load(result.vm, bpf_program__insns(program),
			      (uint32_t)(bpf_program__insn_cnt(program) * 8), &error) != 0) {
		std::string message = error ? error : "failed to load VM";
		std::free(error);
		bpf_object__close(object);
		throw std::runtime_error(message);
	}
	function = ebpf_compile(result.vm, &error);
	bpf_object__close(object);
	if (!function) {
		std::string message = error ? error : "failed to JIT VM";
		std::free(error);
		throw std::runtime_error(message);
	}
	return result;
}

int map_type(map_kind kind)
{
	switch (kind) {
	case map_kind::array: return BPF_MAP_TYPE_ARRAY;
	case map_kind::percpu_array: return BPF_MAP_TYPE_PERCPU_ARRAY;
	case map_kind::hash: return BPF_MAP_TYPE_HASH;
	case map_kind::percpu_hash: return BPF_MAP_TYPE_PERCPU_HASH;
	}
	return BPF_MAP_TYPE_ARRAY;
}

void prime_hash()
{
	auto &shm = bpftime::shm_holder.global_shared_memory;
	for (uint32_t key = 0; key < kActiveKeys; ++key) {
		uint64_t value = key;
		if (shm.bpf_map_update_elem(kMapFd, &key, &value, BPF_ANY, false) != 0)
			throw std::runtime_error("failed to prime hash map");
	}
}

NOINLINE uint64_t run(ebpf_jit_fn function, uint64_t invocations)
{
	uint64_t sink = 0;
	uint64_t context = 1;
	for (uint64_t i = 0; i < invocations; ++i)
		sink += function(&context, sizeof(context));
	return sink;
}

uint64_t timed(ebpf_jit_fn function, uint64_t invocations)
{
	auto begin = std::chrono::steady_clock::now();
	volatile uint64_t sink = run(function, invocations);
	auto end = std::chrono::steady_clock::now();
	(void)sink;
	return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
		end - begin).count();
}

} // namespace

int main(int argc, char **argv)
{
	options opt = parse_options(argc, argv);
	pin_to_cpu(opt.cpu);
	std::string shm_name = "bpftime_percpu_helper_jit_" + std::to_string(getpid());
	setenv("BPFTIME_GLOBAL_SHM_NAME", shm_name.c_str(), 1);
	try {
		bpftime_remove_global_shm();
		bpftime_initialize_global_shm(bpftime::shm_open_type::SHM_REMOVE_AND_CREATE);
		bpftime::bpf_map_attr attr{
			.type = map_type(opt.kind), .key_size = sizeof(uint32_t),
			.value_size = sizeof(uint64_t), .max_ents = 1024};
		if (bpftime_maps_create(kMapFd, "percpu_helper_jit", attr) != kMapFd)
			throw std::runtime_error("failed to create map");
		if (opt.kind == map_kind::hash || opt.kind == map_kind::percpu_hash)
			prime_hash();

		ebpf_jit_fn noop = nullptr, real = nullptr;
		vm_holder noop_vm, real_vm;
		if (opt.mode == "pair" || opt.mode == "noop")
			noop_vm = create_vm(opt, false, noop);
		if (opt.mode == "pair" || opt.mode == "real")
			real_vm = create_vm(opt, true, real);

		std::printf("map_kind=%s operation=%s mode=%s invocations=%" PRIu64
			    " helpers_per_invocation=1000 rounds=%u cpu=%d\n",
			    argv[1], opt.operation.c_str(), opt.mode.c_str(),
			    opt.invocations, opt.rounds, opt.cpu);
		if (opt.mode == "pair") {
			for (unsigned round = 1; round <= opt.rounds; ++round) {
				uint64_t noop_ns, real_ns;
				if (round & 1) {
					noop_ns = timed(noop, opt.invocations);
					real_ns = timed(real, opt.invocations);
				} else {
					real_ns = timed(real, opt.invocations);
					noop_ns = timed(noop, opt.invocations);
				}
				double helpers = (double)opt.invocations * 1000.0;
				std::printf("round=%u noop_ns_per_helper=%.9f real_ns_per_helper=%.9f "
					    "net_ns_per_helper=%.9f\n", round,
					    (double)noop_ns / helpers, (double)real_ns / helpers,
					    ((double)real_ns - (double)noop_ns) / helpers);
			}
		} else {
			ebpf_jit_fn function = opt.mode == "noop" ? noop : real;
			volatile uint64_t sink = run(function, 1000);
			for (unsigned round = 1; round <= opt.rounds; ++round) {
				uint64_t elapsed = timed(function, opt.invocations);
				std::printf("round=%u elapsed_ns=%" PRIu64 " ns_per_helper=%.9f sink=%" PRIu64 "\n",
					    round, elapsed,
					    (double)elapsed / ((double)opt.invocations * 1000.0),
					    (uint64_t)sink);
			}
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

// SPDX-License-Identifier: MIT
//
// Diagnostic-only harness for decomposing bpftime's ordinary array-map path.
// Each layer uses the same map and differs only in the entry point used:
//   control: indirect benchmark-call overhead, no map operation
//   l0:      array_map_impl::{elem_lookup,elem_update}
//   l1:      bpf_map_handler::{map_lookup_elem,map_update_elem}
//   l2:      bpftime_shm::{bpf_map_lookup_elem,bpf_map_update_elem}
//   l3:      bpftime_map_{lookup,update}_elem_helper

#include "bpftime_shm.hpp"
#include "bpftime_shm_internal.hpp"
#include "handler/map_handler.hpp"

#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <optional>
#include <string>
#include <variant>

#include <linux/bpf.h>
#include <sched.h>
#include <unistd.h>

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
constexpr uint32_t kMaxEntries = 1024;
constexpr uint64_t kDefaultOperations = 100000000;
constexpr unsigned kDefaultRounds = 5;

struct context {
	bpftime::array_map_impl *impl;
	const bpftime::bpf_map_handler *handler;
	bpftime::bpftime_shm *shm;
	int fd;
};

using lookup_fn = uint64_t (*)(const context &, const uint32_t *);
using update_fn = uint64_t (*)(const context &, const uint32_t *,
			       const uint64_t *);

#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

NOINLINE uint64_t control_lookup(const context &, const uint32_t *key)
{
	return reinterpret_cast<uint64_t>(key);
}

NOINLINE uint64_t l0_lookup(const context &ctx, const uint32_t *key)
{
	return reinterpret_cast<uint64_t>(ctx.impl->elem_lookup(key));
}

NOINLINE uint64_t l1_lookup(const context &ctx, const uint32_t *key)
{
	return reinterpret_cast<uint64_t>(ctx.handler->map_lookup_elem(key, false));
}

NOINLINE uint64_t l2_lookup(const context &ctx, const uint32_t *key)
{
	return reinterpret_cast<uint64_t>(
		ctx.shm->bpf_map_lookup_elem(ctx.fd, key, false));
}

NOINLINE uint64_t l3_lookup(const context &ctx, const uint32_t *key)
{
	return bpftime_map_lookup_elem_helper(
		static_cast<uint64_t>(ctx.fd), reinterpret_cast<uint64_t>(key), 0,
		0, 0);
}

NOINLINE uint64_t control_update(const context &, const uint32_t *,
				 const uint64_t *value)
{
	return *value & 0;
}

NOINLINE uint64_t l0_update(const context &ctx, const uint32_t *key,
			    const uint64_t *value)
{
	return static_cast<uint64_t>(ctx.impl->elem_update(key, value, BPF_ANY));
}

NOINLINE uint64_t l1_update(const context &ctx, const uint32_t *key,
			    const uint64_t *value)
{
	return static_cast<uint64_t>(
		ctx.handler->map_update_elem(key, value, BPF_ANY, false));
}

NOINLINE uint64_t l2_update(const context &ctx, const uint32_t *key,
			    const uint64_t *value)
{
	return static_cast<uint64_t>(
		ctx.shm->bpf_map_update_elem(ctx.fd, key, value, BPF_ANY, false));
}

NOINLINE uint64_t l3_update(const context &ctx, const uint32_t *key,
			    const uint64_t *value)
{
	return bpftime_map_update_elem_helper(
		static_cast<uint64_t>(ctx.fd), reinterpret_cast<uint64_t>(key),
		reinterpret_cast<uint64_t>(value), BPF_ANY, 0);
}

struct options {
	std::string layer;
	std::string operation;
	uint64_t operations = kDefaultOperations;
	unsigned rounds = kDefaultRounds;
	std::optional<int> cpu;
};

[[noreturn]] void usage(const char *program, const char *error = nullptr)
{
	if (error)
		std::fprintf(stderr, "error: %s\n", error);
	std::fprintf(stderr,
		     "usage: %s <control|l0|l1|l2|l3> <lookup|update> "
		     "[operations] [rounds] [cpu]\n",
		     program);
	std::exit(2);
}

uint64_t parse_u64(const char *text, const char *field,
		   bool allow_zero = false)
{
	char *end = nullptr;
	errno = 0;
	auto result = std::strtoull(text, &end, 10);
	if (errno || !end || *end != '\0' || (!allow_zero && result == 0))
		usage("array-map-path-layers", field);
	return result;
}

options parse_options(int argc, char **argv)
{
	if (argc < 3 || argc > 6)
		usage(argv[0]);
	options result{ .layer = argv[1], .operation = argv[2] };
	if (result.layer != "control" && result.layer != "l0" &&
	    result.layer != "l1" && result.layer != "l2" &&
	    result.layer != "l3")
		usage(argv[0], "unknown layer");
	if (result.operation != "lookup" && result.operation != "update")
		usage(argv[0], "unknown operation");
	if (argc >= 4)
		result.operations = parse_u64(argv[3], "invalid operation count");
	if (argc >= 5)
		result.rounds = static_cast<unsigned>(
			parse_u64(argv[4], "invalid round count"));
	if (argc >= 6)
		result.cpu = static_cast<int>(
			parse_u64(argv[5], "invalid CPU", true));
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

lookup_fn select_lookup(const std::string &layer)
{
	if (layer == "control")
		return control_lookup;
	if (layer == "l0")
		return l0_lookup;
	if (layer == "l1")
		return l1_lookup;
	if (layer == "l2")
		return l2_lookup;
	return l3_lookup;
}

update_fn select_update(const std::string &layer)
{
	if (layer == "control")
		return control_update;
	if (layer == "l0")
		return l0_update;
	if (layer == "l1")
		return l1_update;
	if (layer == "l2")
		return l2_update;
	return l3_update;
}

template <typename Function>
uint64_t run_lookup(Function fn, const context &ctx, uint64_t operations)
{
	uint64_t sink = 0;
	uint64_t completed = 0;
	while (completed < operations) {
		const uint64_t remaining = operations - completed;
		const uint32_t count = static_cast<uint32_t>(
			remaining < 1000 ? remaining : 1000);
		for (uint32_t key = 0; key < count; ++key)
			sink ^= fn(ctx, &key);
		completed += count;
	}
	return sink;
}

template <typename Function>
uint64_t run_update(Function fn, const context &ctx, uint64_t operations)
{
	uint64_t sink = 0;
	uint64_t completed = 0;
	while (completed < operations) {
		const uint64_t remaining = operations - completed;
		const uint32_t count = static_cast<uint32_t>(
			remaining < 1000 ? remaining : 1000);
		for (uint32_t key = 0; key < count; ++key) {
			const uint64_t value = key;
			sink += fn(ctx, &key, &value);
		}
		completed += count;
	}
	return sink;
}

} // namespace

int main(int argc, char **argv)
{
	const options opt = parse_options(argc, argv);
	if (opt.cpu)
		pin_to_cpu(*opt.cpu);

	const std::string shm_name =
		"bpftime_array_path_layers_" + std::to_string(getpid());
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
			.max_ents = kMaxEntries,
		};
		if (bpftime_maps_create(kMapFd, "array_path_layers", attr) !=
		    kMapFd) {
			std::fprintf(stderr, "failed to create array map\n");
			bpftime_destroy_global_shm();
			bpftime_remove_global_shm();
			return 1;
		}

		auto &shm = bpftime::shm_holder.global_shared_memory;
		auto impl = shm.try_get_array_map_impl(kMapFd);
		if (!impl) {
			std::fprintf(stderr, "failed to resolve array implementation\n");
			return 1;
		}
		const auto &handler =
			std::get<bpftime::bpf_map_handler>(shm.get_handler(kMapFd));
		const context ctx{ .impl = *impl,
				   .handler = &handler,
				   .shm = &shm,
				   .fd = kMapFd };

		lookup_fn lookup = select_lookup(opt.layer);
		update_fn update = select_update(opt.layer);

		// One untimed warm-up uses the same access pattern as a measured round.
		const uint64_t warmup_operations =
			opt.operations < 1000000 ? opt.operations : 1000000;
		volatile uint64_t sink =
			opt.operation == "lookup" ?
				run_lookup(lookup, ctx, warmup_operations) :
				run_update(update, ctx, warmup_operations);

		std::printf("layer=%s operation=%s operations=%" PRIu64
			    " rounds=%u cpu=%d\n",
			    opt.layer.c_str(), opt.operation.c_str(), opt.operations,
			    opt.rounds, sched_getcpu());
		for (unsigned round = 1; round <= opt.rounds; ++round) {
			const auto begin = std::chrono::steady_clock::now();
			if (opt.operation == "lookup")
				sink ^= run_lookup(lookup, ctx, opt.operations);
			else
				sink += run_update(update, ctx, opt.operations);
			const auto end = std::chrono::steady_clock::now();
			const uint64_t elapsed_ns =
				std::chrono::duration_cast<std::chrono::nanoseconds>(
					end - begin)
					.count();
			std::printf("round=%u elapsed_ns=%" PRIu64
				    " ns_per_op=%.6f sink=%" PRIu64 "\n",
				    round, elapsed_ns,
				    static_cast<double>(elapsed_ns) /
					    static_cast<double>(opt.operations),
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

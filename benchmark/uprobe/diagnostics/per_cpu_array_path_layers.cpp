// SPDX-License-Identifier: MIT
// Diagnostic-only harness for decomposing the userspace per-CPU array-map
// lookup/update path.  It deliberately does not change the runtime.
//
// The synthetic layers mirror the body in
// runtime/src/bpf_map/userspace/per_cpu_array_map.cpp:
//
//   fixed        bounds check + per-CPU address calculation, fixed CPU
//   sched        the same body after my_sched_getcpu()
//   std_function  the same body through ensure_on_current_cpu<T>()
//
// l0 is the production per_cpu_array_map_impl method.  l1/l2/l3 add the
// handler, shared-memory, and helper-dispatch layers respectively.

#include "bpftime_shm.hpp"
#include "bpftime_shm_internal.hpp"
#include "bpf_map/map_common_def.hpp"
#include "bpf_map/userspace/per_cpu_array_map.hpp"
#include "handler/map_handler.hpp"

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/shared_memory_object.hpp>

#include <algorithm>
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
constexpr uint32_t kValueSize = sizeof(uint64_t);
constexpr uint64_t kDefaultOperations = 100000000;
constexpr unsigned kDefaultRounds = 5;
constexpr std::size_t kLocalShmSize = 32ULL << 20;

#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

struct context {
	bpftime::per_cpu_array_map_impl *direct;
	const bpftime::bpf_map_handler *handler;
	bpftime::bpftime_shm *shm;
	int fd;
	bpftime::bytes_vec *synthetic_data;
	uint32_t ncpu;
	uint32_t value_size;
	uint32_t max_entries;
	int fixed_cpu;
};

using lookup_fn = uint64_t (*)(const context &, const uint32_t *);
using update_fn = uint64_t (*)(const context &, const uint32_t *,
				       const uint64_t *);

NOINLINE uint64_t control_lookup(const context &, const uint32_t *key)
{
	return reinterpret_cast<uint64_t>(key);
}

NOINLINE uint64_t fixed_lookup(const context &ctx, const uint32_t *key)
{
	if (key == nullptr)
		return 0;
	const uint32_t key_val = *key;
	if (key_val >= ctx.max_entries)
		return 0;
	return reinterpret_cast<uint64_t>(ctx.synthetic_data->data() +
					 key_val * ctx.value_size * ctx.ncpu +
					 ctx.fixed_cpu * ctx.value_size);
}

NOINLINE uint64_t sched_lookup(const context &ctx, const uint32_t *key)
{
	const int cpu = my_sched_getcpu();
	if (key == nullptr)
		return 0;
	const uint32_t key_val = *key;
	if (key_val >= ctx.max_entries)
		return 0;
	return reinterpret_cast<uint64_t>(ctx.synthetic_data->data() +
					 key_val * ctx.value_size * ctx.ncpu +
					 cpu * ctx.value_size);
}

NOINLINE uint64_t std_function_lookup(const context &ctx,
					      const uint32_t *key)
{
	return reinterpret_cast<uint64_t>(bpftime::ensure_on_current_cpu<void *>(
		[&](int cpu) -> void * {
			if (key == nullptr)
				return nullptr;
			const uint32_t key_val = *key;
			if (key_val >= ctx.max_entries)
				return nullptr;
			return ctx.synthetic_data->data() +
				       key_val * ctx.value_size * ctx.ncpu +
				       cpu * ctx.value_size;
		}));
}

NOINLINE uint64_t l0_lookup(const context &ctx, const uint32_t *key)
{
	return reinterpret_cast<uint64_t>(ctx.direct->elem_lookup(key));
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

NOINLINE uint64_t fixed_update(const context &ctx, const uint32_t *key,
				       const uint64_t *value)
{
	if (!bpftime::check_update_flags(BPF_ANY) || key == nullptr)
		return static_cast<uint64_t>(-1);
	const uint32_t key_val = *key;
	if (key_val >= ctx.max_entries)
		return static_cast<uint64_t>(-1);
	std::copy(reinterpret_cast<const uint8_t *>(value),
		  reinterpret_cast<const uint8_t *>(value) + ctx.value_size,
		  ctx.synthetic_data->data() +
			  key_val * ctx.value_size * ctx.ncpu +
			  ctx.fixed_cpu * ctx.value_size);
	return 0;
}

NOINLINE uint64_t sched_update(const context &ctx, const uint32_t *key,
				       const uint64_t *value)
{
	const int cpu = my_sched_getcpu();
	if (!bpftime::check_update_flags(BPF_ANY) || key == nullptr)
		return static_cast<uint64_t>(-1);
	const uint32_t key_val = *key;
	if (key_val >= ctx.max_entries)
		return static_cast<uint64_t>(-1);
	std::copy(reinterpret_cast<const uint8_t *>(value),
		  reinterpret_cast<const uint8_t *>(value) + ctx.value_size,
		  ctx.synthetic_data->data() +
			  key_val * ctx.value_size * ctx.ncpu + cpu * ctx.value_size);
	return 0;
}

NOINLINE uint64_t std_function_update(const context &ctx,
					      const uint32_t *key,
					      const uint64_t *value)
{
	if (!bpftime::check_update_flags(BPF_ANY))
		return static_cast<uint64_t>(-1);
	return static_cast<uint64_t>(bpftime::ensure_on_current_cpu<long>(
		[&](int cpu) -> long {
			if (key == nullptr)
				return -1;
			const uint32_t key_val = *key;
			if (key_val >= ctx.max_entries)
				return -1;
			std::copy(reinterpret_cast<const uint8_t *>(value),
				  reinterpret_cast<const uint8_t *>(value) +
					  ctx.value_size,
				  ctx.synthetic_data->data() +
					  key_val * ctx.value_size * ctx.ncpu +
					  cpu * ctx.value_size);
			return 0;
		}));
}

NOINLINE uint64_t l0_update(const context &ctx, const uint32_t *key,
				    const uint64_t *value)
{
	return static_cast<uint64_t>(ctx.direct->elem_update(key, value, BPF_ANY));
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
		     "usage: %s <control|fixed|sched|std_function|l0|l1|l2|l3> "
		     "<lookup|update> [operations] [rounds] [cpu]\n",
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
		usage("per-cpu-array-path-layers", field);
	return result;
}

options parse_options(int argc, char **argv)
{
	if (argc < 3 || argc > 6)
		usage(argv[0]);
	options result{ .layer = argv[1], .operation = argv[2] };
	if (result.layer != "control" && result.layer != "fixed" &&
	    result.layer != "sched" && result.layer != "std_function" &&
	    result.layer != "l0" && result.layer != "l1" &&
	    result.layer != "l2" && result.layer != "l3")
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
	if (layer == "fixed")
		return fixed_lookup;
	if (layer == "sched")
		return sched_lookup;
	if (layer == "std_function")
		return std_function_lookup;
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
	if (layer == "fixed")
		return fixed_update;
	if (layer == "sched")
		return sched_update;
	if (layer == "std_function")
		return std_function_update;
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

	const std::string suffix = std::to_string(getpid());
	const std::string global_name =
		"bpftime_percpu_array_path_layers_" + suffix;
	const std::string local_name = "bpftime_percpu_array_local_" + suffix;
	if (setenv("BPFTIME_GLOBAL_SHM_NAME", global_name.c_str(), 1) != 0) {
		std::perror("setenv");
		return 1;
	}
	boost::interprocess::shared_memory_object::remove(local_name.c_str());

	try {
		bpftime_remove_global_shm();
		bpftime_initialize_global_shm(
			bpftime::shm_open_type::SHM_REMOVE_AND_CREATE);
		const bpftime::bpf_map_attr attr{
			.type = static_cast<int>(
				bpftime::bpf_map_type::BPF_MAP_TYPE_PERCPU_ARRAY),
			.key_size = sizeof(uint32_t),
			.value_size = kValueSize,
			.max_ents = kMaxEntries,
		};
		if (bpftime_maps_create(kMapFd, "percpu_array_path_layers", attr) !=
		    kMapFd)
			throw std::runtime_error("failed to create per-CPU array map");

		auto &shm = bpftime::shm_holder.global_shared_memory;
		const auto &handler =
			std::get<bpftime::bpf_map_handler>(shm.get_handler(kMapFd));
		const uint32_t ncpu = static_cast<uint32_t>(
			sysconf(_SC_NPROCESSORS_ONLN));
		const int fixed_cpu = opt.cpu.value_or(sched_getcpu());

		{
			boost::interprocess::managed_shared_memory local_mem(
				boost::interprocess::create_only, local_name.c_str(),
				kLocalShmSize);
			bpftime::per_cpu_array_map_impl direct(
				local_mem, kValueSize, kMaxEntries, ncpu);
			bpftime::bytes_vec synthetic(local_mem.get_segment_manager());
			synthetic.resize(static_cast<std::size_t>(kMaxEntries) * ncpu *
					 kValueSize);
			const context ctx{ .direct = &direct,
					   .handler = &handler,
					   .shm = &shm,
					   .fd = kMapFd,
					   .synthetic_data = &synthetic,
					   .ncpu = ncpu,
					   .value_size = kValueSize,
					   .max_entries = kMaxEntries,
					   .fixed_cpu = fixed_cpu };

			lookup_fn lookup = select_lookup(opt.layer);
			update_fn update = select_update(opt.layer);
			const uint64_t warmup_operations =
				opt.operations < 1000000 ? opt.operations : 1000000;
			volatile uint64_t sink =
				opt.operation == "lookup" ?
					run_lookup(lookup, ctx, warmup_operations) :
					run_update(update, ctx, warmup_operations);

			std::printf(
				"layer=%s operation=%s operations=%" PRIu64
				" rounds=%u cpu=%d ncpu=%u value_size=%u\n",
				opt.layer.c_str(), opt.operation.c_str(), opt.operations,
				opt.rounds, sched_getcpu(), ncpu, kValueSize);
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
				std::printf(
					"round=%u elapsed_ns=%" PRIu64
					" ns_per_op=%.6f sink=%" PRIu64 "\n",
					round, elapsed_ns,
					static_cast<double>(elapsed_ns) /
						static_cast<double>(opt.operations),
					static_cast<uint64_t>(sink));
			}
		}

		bpftime_destroy_global_shm();
		bpftime_remove_global_shm();
		boost::interprocess::shared_memory_object::remove(local_name.c_str());
	} catch (const std::exception &error) {
		std::fprintf(stderr, "fatal: %s\n", error.what());
		bpftime_destroy_global_shm();
		bpftime_remove_global_shm();
		boost::interprocess::shared_memory_object::remove(local_name.c_str());
		return 1;
	}
	return 0;
}

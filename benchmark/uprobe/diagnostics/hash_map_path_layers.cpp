// SPDX-License-Identifier: MIT
// Diagnostic-only harness for decomposing bpftime's ordinary fixed hash-map
// path. The steady states match benchmark/uprobe/test.c:
//   update -> existing key, lookup -> hit, delete -> miss after the first pass.

#include "bpftime_shm.hpp"
#include "bpftime_shm_internal.hpp"
#include "bpf_map/userspace/fix_hash_map.hpp"
#include "handler/map_handler.hpp"

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/shared_memory_object.hpp>

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
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

extern "C" {
uint64_t bpftime_map_lookup_elem_helper(uint64_t map, uint64_t key,
					uint64_t, uint64_t, uint64_t);
uint64_t bpftime_map_update_elem_helper(uint64_t map, uint64_t key,
					uint64_t value, uint64_t flags,
					uint64_t);
uint64_t bpftime_map_delete_elem_helper(uint64_t map, uint64_t key,
					uint64_t, uint64_t, uint64_t);
}

namespace
{

constexpr int kMapFd = 3;
constexpr uint32_t kMaxEntries = 1024;
constexpr uint32_t kActiveKeys = 1000;
constexpr uint64_t kDefaultOperations = 10000000;
constexpr unsigned kDefaultRounds = 5;

struct context {
	bpftime::fix_size_hash_map_impl *impl;
	const bpftime::bpf_map_handler *handler;
	bpftime::bpftime_shm *shm;
	int fd;
};

using lookup_fn = uint64_t (*)(const context &, const uint32_t *);
using update_fn = uint64_t (*)(const context &, const uint32_t *,
			       const uint64_t *);
using delete_fn = uint64_t (*)(const context &, const uint32_t *);

#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

NOINLINE uint64_t control_lookup(const context &, const uint32_t *key)
{
	return reinterpret_cast<uint64_t>(key);
}

NOINLINE uint64_t lock_lookup(const context &ctx, const uint32_t *)
{
	auto &lock = ctx.handler->get_raw_spin_lock();
	pthread_spin_lock(&lock);
	pthread_spin_unlock(&lock);
	return 0;
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

NOINLINE uint64_t lock_update(const context &ctx, const uint32_t *,
			      const uint64_t *)
{
	auto &lock = ctx.handler->get_raw_spin_lock();
	pthread_spin_lock(&lock);
	pthread_spin_unlock(&lock);
	return 0;
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

NOINLINE uint64_t control_delete(const context &, const uint32_t *)
{
	return 0;
}

NOINLINE uint64_t lock_delete(const context &ctx, const uint32_t *)
{
	auto &lock = ctx.handler->get_raw_spin_lock();
	pthread_spin_lock(&lock);
	pthread_spin_unlock(&lock);
	return 0;
}

NOINLINE uint64_t l0_delete(const context &ctx, const uint32_t *key)
{
	return static_cast<uint64_t>(ctx.impl->elem_delete(key));
}

NOINLINE uint64_t l1_delete(const context &ctx, const uint32_t *key)
{
	return static_cast<uint64_t>(ctx.handler->map_delete_elem(key, false));
}

NOINLINE uint64_t l2_delete(const context &ctx, const uint32_t *key)
{
	return static_cast<uint64_t>(
		ctx.shm->bpf_delete_elem(ctx.fd, key, false));
}

NOINLINE uint64_t l3_delete(const context &ctx, const uint32_t *key)
{
	return bpftime_map_delete_elem_helper(
		static_cast<uint64_t>(ctx.fd), reinterpret_cast<uint64_t>(key), 0,
		0, 0);
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
		     "usage: %s <control|lock|l0|l1|l2|l3> "
		     "<lookup|update|delete> [operations] [rounds] [cpu]\n",
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
		usage("hash-map-path-layers", field);
	return result;
}

options parse_options(int argc, char **argv)
{
	if (argc < 3 || argc > 6)
		usage(argv[0]);
	options result{ .layer = argv[1], .operation = argv[2] };
	if (result.layer != "control" && result.layer != "lock" &&
	    result.layer != "l0" && result.layer != "l1" &&
	    result.layer != "l2" && result.layer != "l3")
		usage(argv[0], "unknown layer");
	if (result.operation != "lookup" && result.operation != "update" &&
	    result.operation != "delete")
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
	if (layer == "control") return control_lookup;
	if (layer == "lock") return lock_lookup;
	if (layer == "l0") return l0_lookup;
	if (layer == "l1") return l1_lookup;
	if (layer == "l2") return l2_lookup;
	return l3_lookup;
}

update_fn select_update(const std::string &layer)
{
	if (layer == "control") return control_update;
	if (layer == "lock") return lock_update;
	if (layer == "l0") return l0_update;
	if (layer == "l1") return l1_update;
	if (layer == "l2") return l2_update;
	return l3_update;
}

delete_fn select_delete(const std::string &layer)
{
	if (layer == "control") return control_delete;
	if (layer == "lock") return lock_delete;
	if (layer == "l0") return l0_delete;
	if (layer == "l1") return l1_delete;
	if (layer == "l2") return l2_delete;
	return l3_delete;
}

template <typename Function>
uint64_t run_lookup_or_delete(Function fn, const context &ctx,
			      uint64_t operations)
{
	uint64_t sink = 0;
	uint64_t completed = 0;
	while (completed < operations) {
		const uint32_t count = static_cast<uint32_t>(
			operations - completed < kActiveKeys ?
				operations - completed : kActiveKeys);
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
		const uint32_t count = static_cast<uint32_t>(
			operations - completed < kActiveKeys ?
				operations - completed : kActiveKeys);
		for (uint32_t key = 0; key < count; ++key) {
			const uint64_t value = key;
			sink += fn(ctx, &key, &value);
		}
		completed += count;
	}
	return sink;
}

void prime_maps(bpftime::fix_size_hash_map_impl &impl,
		const bpftime::bpf_map_handler &handler)
{
	for (uint32_t key = 0; key < kActiveKeys; ++key) {
		const uint64_t value = key;
		if (impl.elem_update(&key, &value, BPF_ANY) != 0 ||
		    handler.map_update_elem(&key, &value, BPF_ANY, false) != 0)
			throw std::runtime_error("failed to prime hash maps");
	}
}

} // namespace

int main(int argc, char **argv)
{
	const options opt = parse_options(argc, argv);
	if (opt.cpu)
		pin_to_cpu(*opt.cpu);

	const std::string suffix = std::to_string(getpid());
	const std::string global_name = "bpftime_hash_path_layers_" + suffix;
	const std::string local_name = "bpftime_hash_l0_" + suffix;
	setenv("BPFTIME_GLOBAL_SHM_NAME", global_name.c_str(), 1);
	boost::interprocess::shared_memory_object::remove(local_name.c_str());

	try {
		boost::interprocess::managed_shared_memory local_segment(
			boost::interprocess::create_only, local_name.c_str(),
			20 * 1024 * 1024);
		bpftime::fix_size_hash_map_impl local_impl(
			local_segment, kMaxEntries, sizeof(uint32_t), sizeof(uint64_t));

		bpftime_remove_global_shm();
		bpftime_initialize_global_shm(
			bpftime::shm_open_type::SHM_REMOVE_AND_CREATE);
		const bpftime::bpf_map_attr attr{
			.type = static_cast<int>(bpftime::bpf_map_type::BPF_MAP_TYPE_HASH),
			.key_size = sizeof(uint32_t),
			.value_size = sizeof(uint64_t),
			.max_ents = kMaxEntries,
		};
		if (bpftime_maps_create(kMapFd, "hash_path_layers", attr) != kMapFd)
			throw std::runtime_error("failed to create hash map");

		auto &shm = bpftime::shm_holder.global_shared_memory;
		const auto &handler =
			std::get<bpftime::bpf_map_handler>(shm.get_handler(kMapFd));
		if (opt.operation != "delete")
			prime_maps(local_impl, handler);
		const context ctx{ .impl = &local_impl,
				   .handler = &handler,
				   .shm = &shm,
				   .fd = kMapFd };

		const lookup_fn lookup = select_lookup(opt.layer);
		const update_fn update = select_update(opt.layer);
		const delete_fn remove = select_delete(opt.layer);
		const uint64_t warmup_operations =
			opt.operations < 1000000 ? opt.operations : 1000000;
		volatile uint64_t sink = 0;
		if (opt.operation == "lookup")
			sink = run_lookup_or_delete(lookup, ctx, warmup_operations);
		else if (opt.operation == "update")
			sink = run_update(update, ctx, warmup_operations);
		else
			sink = run_lookup_or_delete(remove, ctx, warmup_operations);

		const char *state = opt.operation == "lookup" ? "hit" :
				    opt.operation == "update" ? "existing" : "miss";
		std::printf("layer=%s operation=%s state=%s operations=%" PRIu64
			    " rounds=%u cpu=%d\n",
			    opt.layer.c_str(), opt.operation.c_str(), state,
			    opt.operations, opt.rounds, sched_getcpu());
		for (unsigned round = 1; round <= opt.rounds; ++round) {
			const auto begin = std::chrono::steady_clock::now();
			if (opt.operation == "lookup")
				sink ^= run_lookup_or_delete(lookup, ctx, opt.operations);
			else if (opt.operation == "update")
				sink += run_update(update, ctx, opt.operations);
			else
				sink += run_lookup_or_delete(remove, ctx, opt.operations);
			const auto end = std::chrono::steady_clock::now();
			const uint64_t elapsed_ns =
				std::chrono::duration_cast<std::chrono::nanoseconds>(
					end - begin).count();
			std::printf("round=%u elapsed_ns=%" PRIu64
				    " ns_per_op=%.6f sink=%" PRIu64 "\n",
				    round, elapsed_ns,
				    static_cast<double>(elapsed_ns) /
					    static_cast<double>(opt.operations),
				    static_cast<uint64_t>(sink));
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

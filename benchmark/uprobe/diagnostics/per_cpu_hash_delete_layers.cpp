// SPDX-License-Identifier: MIT
// Diagnostic-only benchmark for the per-CPU hash delete-hit path.
//
// The production implementation combines lookup, unlink, node destruction,
// and shared-memory deallocation in boost::unordered_map::erase().  This
// harness keeps the production runtime untouched and uses Boost node handles
// to defer destruction outside the timed region.

#include "bpftime_shm.hpp"
#include "bpftime_shm_internal.hpp"
#include "bpf_map/map_common_def.hpp"
#include "bpf_map/userspace/per_cpu_hash_map.hpp"
#include "handler/map_handler.hpp"

#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/unordered/unordered_map.hpp>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <linux/bpf.h>
#include <sched.h>
#include <unistd.h>

namespace
{
constexpr int kMapFd = 3;
constexpr uint32_t kMaxEntries = 1024;
constexpr uint32_t kActiveKeys = 1000;
constexpr uint32_t kKeySize = sizeof(uint32_t);
constexpr uint32_t kValueSize = sizeof(uint64_t);
constexpr uint64_t kDefaultOperations = kActiveKeys;
constexpr unsigned kDefaultRounds = 5;
constexpr std::size_t kLocalShmSize = 64ULL << 20;

#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

using bytes_vec = bpftime::bytes_vec;
using pair_type = std::pair<const bytes_vec, bytes_vec>;
using pair_allocator = boost::interprocess::allocator<
	pair_type, boost::interprocess::managed_shared_memory::segment_manager>;
using synthetic_map = boost::unordered_map<
	bytes_vec, bytes_vec, bpftime::bytes_vec_hasher, std::equal_to<bytes_vec>,
	pair_allocator>;

struct synthetic_hash {
	synthetic_map map;
	std::vector<bytes_vec> key_templates;
	bytes_vec full_value_template;
	std::vector<typename synthetic_map::node_type> deferred_nodes;
	uint32_t key_size;
	uint32_t value_size;
	uint32_t ncpu;
	int fixed_cpu;

	synthetic_hash(boost::interprocess::managed_shared_memory &memory,
		       uint32_t key_size_in, uint32_t value_size_in,
		       uint32_t max_entries, uint32_t ncpu_in, int fixed_cpu_in)
		: map(max_entries, bpftime::bytes_vec_hasher(),
		      std::equal_to<bytes_vec>(), pair_allocator(memory.get_segment_manager())),
		  full_value_template(value_size_in * ncpu_in,
				      memory.get_segment_manager()),
		  key_size(key_size_in), value_size(value_size_in), ncpu(ncpu_in),
		  fixed_cpu(fixed_cpu_in)
	{
		for (uint32_t cpu = 0; cpu < ncpu; ++cpu)
			key_templates.emplace_back(key_size,
						  memory.get_segment_manager());
		deferred_nodes.reserve(max_entries);
	}
};

struct context {
	bpftime::per_cpu_hash_map_impl *direct;
	const bpftime::bpf_map_handler *handler;
	synthetic_hash *synthetic;
	uint32_t key_size;
};

using delete_fn = uint64_t (*)(const context &, const uint32_t *);

volatile std::sig_atomic_t g_start_signal = 0;
volatile std::sig_atomic_t g_cleanup_signal = 0;

void start_signal_handler(int)
{
	g_start_signal = 1;
}

void cleanup_signal_handler(int)
{
	g_cleanup_signal = 1;
}

void wait_for_signal(const char *phase, bool cleanup)
{
	struct sigaction action {
	};
	action.sa_handler = cleanup ? cleanup_signal_handler : start_signal_handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	if (sigaction(cleanup ? SIGUSR2 : SIGUSR1, &action, nullptr) != 0) {
		std::perror("sigaction");
		std::exit(1);
	}
	std::printf("wait=%s pid=%d signal=%s\n", phase, getpid(),
			cleanup ? "SIGUSR2" : "SIGUSR1");
	std::fflush(stdout);
	volatile std::sig_atomic_t &flag =
		cleanup ? g_cleanup_signal : g_start_signal;
	while (!flag)
		pause();
}

NOINLINE uint64_t control_delete(const context &, const uint32_t *key)
{
	return reinterpret_cast<uint64_t>(key);
}

NOINLINE uint64_t cpu_key_delete(const context &ctx, const uint32_t *key)
{
	const int cpu = my_sched_getcpu();
	auto &key_vec = ctx.synthetic->key_templates[cpu];
	key_vec.assign(reinterpret_cast<const uint8_t *>(key),
		      reinterpret_cast<const uint8_t *>(key) + ctx.key_size);
	return reinterpret_cast<uint64_t>(key_vec.data());
}

NOINLINE uint64_t find_only_delete(const context &ctx, const uint32_t *key)
{
	const int cpu = my_sched_getcpu();
	auto &key_vec = ctx.synthetic->key_templates[cpu];
	key_vec.assign(reinterpret_cast<const uint8_t *>(key),
		      reinterpret_cast<const uint8_t *>(key) + ctx.key_size);
	const auto itr = ctx.synthetic->map.find(key_vec);
	return itr == ctx.synthetic->map.end()
		       ? static_cast<uint64_t>(-1)
		       : reinterpret_cast<uint64_t>(itr->second.data());
}

// Unlink the node from the hash table, but retain it until the timed loop has
// completed.  Node destruction and shared-memory deallocation are therefore
// outside the measured interval.
NOINLINE uint64_t find_extract_hold_delete(const context &ctx,
						 const uint32_t *key)
{
	const int cpu = my_sched_getcpu();
	auto &key_vec = ctx.synthetic->key_templates[cpu];
	key_vec.assign(reinterpret_cast<const uint8_t *>(key),
		      reinterpret_cast<const uint8_t *>(key) + ctx.key_size);
	const auto itr = ctx.synthetic->map.find(key_vec);
	if (itr == ctx.synthetic->map.end())
		return static_cast<uint64_t>(-1);
	auto node = ctx.synthetic->map.extract(itr);
	const uint64_t result = reinterpret_cast<uint64_t>(node.mapped().data());
	ctx.synthetic->deferred_nodes.push_back(std::move(node));
	return result;
}

// Extract and immediately destroy the node.  This is an API-equivalent
// control for the held-node case and keeps unlink separate from destruction.
NOINLINE uint64_t find_extract_drop_delete(const context &ctx,
						 const uint32_t *key)
{
	const int cpu = my_sched_getcpu();
	auto &key_vec = ctx.synthetic->key_templates[cpu];
	key_vec.assign(reinterpret_cast<const uint8_t *>(key),
		      reinterpret_cast<const uint8_t *>(key) + ctx.key_size);
	const auto itr = ctx.synthetic->map.find(key_vec);
	if (itr == ctx.synthetic->map.end())
		return static_cast<uint64_t>(-1);
	auto node = ctx.synthetic->map.extract(itr);
	return reinterpret_cast<uint64_t>(node.mapped().data());
}

NOINLINE uint64_t find_erase_delete(const context &ctx, const uint32_t *key)
{
	const int cpu = my_sched_getcpu();
	auto &key_vec = ctx.synthetic->key_templates[cpu];
	key_vec.assign(reinterpret_cast<const uint8_t *>(key),
		      reinterpret_cast<const uint8_t *>(key) + ctx.key_size);
	const auto itr = ctx.synthetic->map.find(key_vec);
	if (itr == ctx.synthetic->map.end())
		return static_cast<uint64_t>(-1);
	ctx.synthetic->map.erase(itr);
	return 0;
}

NOINLINE uint64_t production_l0_delete(const context &ctx,
					       const uint32_t *key)
{
	return static_cast<uint64_t>(ctx.direct->elem_delete(key));
}

struct options {
	std::string layer;
	uint64_t operations = kDefaultOperations;
	unsigned rounds = kDefaultRounds;
	std::optional<int> cpu;
};

[[noreturn]] void usage(const char *program, const char *error = nullptr)
{
	if (error)
		std::fprintf(stderr, "error: %s\n", error);
	std::fprintf(stderr,
		     "usage: %s <control|cpu_key|find_only|find_extract_hold|"
		     "find_extract_drop|find_erase|l0> [operations] [rounds] [cpu]\n",
		     program);
	std::exit(2);
}

uint64_t parse_u64(const char *text, const char *field,
			   bool allow_zero = false)
{
	char *end = nullptr;
	errno = 0;
	const auto result = std::strtoull(text, &end, 10);
	if (errno || !end || *end != '\0' || (!allow_zero && result == 0))
		usage("per-cpu-hash-delete-layers", field);
	return result;
}

options parse_options(int argc, char **argv)
{
	if (argc < 2 || argc > 5)
		usage(argv[0]);
	options result{ .layer = argv[1] };
	if (result.layer != "control" && result.layer != "cpu_key" &&
	    result.layer != "find_only" && result.layer != "find_extract_hold" &&
	    result.layer != "find_extract_drop" && result.layer != "find_erase" &&
	    result.layer != "l0")
		usage(argv[0], "unknown layer");
	if (argc >= 3)
		result.operations = parse_u64(argv[2], "invalid operation count");
	if (argc >= 4)
		result.rounds = static_cast<unsigned>(
			parse_u64(argv[3], "invalid round count"));
	if (argc >= 5)
		result.cpu = static_cast<int>(parse_u64(argv[4], "invalid CPU", true));
	if (result.operations > kActiveKeys)
		usage(argv[0], "delete-hit requires operations <= 1000");
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

delete_fn select_delete(const std::string &layer)
{
	if (layer == "control") return control_delete;
	if (layer == "cpu_key") return cpu_key_delete;
	if (layer == "find_only") return find_only_delete;
	if (layer == "find_extract_hold") return find_extract_hold_delete;
	if (layer == "find_extract_drop") return find_extract_drop_delete;
	if (layer == "find_erase") return find_erase_delete;
	return production_l0_delete;
}

template <typename Function>
uint64_t run_delete(Function fn, const context &ctx, uint64_t operations)
{
	uint64_t sink = 0;
	for (uint32_t key = 0; key < operations; ++key)
		sink ^= fn(ctx, &key);
	return sink;
}

void prime_synthetic(synthetic_hash &synthetic)
{
	synthetic.deferred_nodes.clear();
	synthetic.map.clear();
	for (uint32_t key = 0; key < kActiveKeys; ++key) {
		bytes_vec key_vec(synthetic.key_size,
				  synthetic.map.get_allocator().get_segment_manager());
		std::memcpy(key_vec.data(), &key, synthetic.key_size);
		bytes_vec value_vec = synthetic.full_value_template;
		const uint64_t value = key;
		std::memcpy(value_vec.data() + synthetic.fixed_cpu * synthetic.value_size,
			    &value, synthetic.value_size);
		synthetic.map.insert(std::make_pair(std::move(key_vec),
						    std::move(value_vec)));
	}
}

void prime_production(bpftime::per_cpu_hash_map_impl &direct,
			      const bpftime::bpf_map_handler &handler)
{
	for (uint32_t key = 0; key < kActiveKeys; ++key) {
		const uint64_t value = key;
		direct.elem_update(&key, &value, BPF_ANY);
		const_cast<bpftime::bpf_map_handler &>(handler).map_update_elem(
			&key, &value, BPF_ANY, false);
	}
}

} // namespace

int main(int argc, char **argv)
{
	const options opt = parse_options(argc, argv);
	const bool wait_mode = std::getenv("BPFTIME_DELETE_LAYERS_WAIT") != nullptr;
	if (opt.cpu)
		pin_to_cpu(*opt.cpu);

	const std::string suffix = std::to_string(getpid());
	const std::string global_name = "bpftime_percpu_hash_delete_" + suffix;
	const std::string local_name = "bpftime_percpu_hash_delete_local_" + suffix;
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
			.type = static_cast<int>(bpftime::bpf_map_type::BPF_MAP_TYPE_PERCPU_HASH),
			.key_size = kKeySize,
			.value_size = kValueSize,
			.max_ents = kMaxEntries,
		};
		if (bpftime_maps_create(kMapFd, "percpu_hash_delete_layers", attr) !=
		    kMapFd)
			throw std::runtime_error("failed to create per-CPU hash map");

		auto &shm = bpftime::shm_holder.global_shared_memory;
		const auto &handler = std::get<bpftime::bpf_map_handler>(
			shm.get_handler(kMapFd));
		const uint32_t ncpu = static_cast<uint32_t>(
			sysconf(_SC_NPROCESSORS_ONLN));
		const int fixed_cpu = opt.cpu.value_or(sched_getcpu());

		boost::interprocess::managed_shared_memory local_mem(
			boost::interprocess::create_only, local_name.c_str(), kLocalShmSize);
		bpftime::per_cpu_hash_map_impl direct(
			local_mem, kKeySize, kValueSize, kMaxEntries, ncpu);
		synthetic_hash synthetic(local_mem, kKeySize, kValueSize, kMaxEntries,
					 ncpu, fixed_cpu);
		const context ctx{ .direct = &direct,
				   .handler = &handler,
				   .synthetic = &synthetic,
				   .key_size = kKeySize };

		prime_synthetic(synthetic);
		prime_production(direct, handler);
		volatile uint64_t sink = 0;
		if (opt.layer == "l0")
			sink = run_delete(production_l0_delete, ctx, kActiveKeys);
		else
			sink = run_delete(select_delete(opt.layer), ctx, kActiveKeys);
		prime_synthetic(synthetic);
		prime_production(direct, handler);
		if (wait_mode)
			wait_for_signal("before-timed-delete", false);

		std::printf("layer=%s operation=delete-hit operations=%" PRIu64
			    " rounds=%u cpu=%d ncpu=%u key_size=%u value_size=%u\n",
			    opt.layer.c_str(), opt.operations, opt.rounds, sched_getcpu(),
			    ncpu, kKeySize, kValueSize);
		for (unsigned round = 1; round <= opt.rounds; ++round) {
			prime_synthetic(synthetic);
			prime_production(direct, handler);
			const auto begin = std::chrono::steady_clock::now();
			if (opt.layer == "l0")
				sink ^= run_delete(production_l0_delete, ctx, opt.operations);
			else
				sink ^= run_delete(select_delete(opt.layer), ctx,
						  opt.operations);
			const auto end = std::chrono::steady_clock::now();
			const uint64_t elapsed_ns =
				std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin)
					.count();
			std::printf("round=%u elapsed_ns=%" PRIu64 " ns_per_op=%.6f sink=%" PRIu64
				    " deferred=%zu\n",
				    round, elapsed_ns,
				    static_cast<double>(elapsed_ns) /
					static_cast<double>(opt.operations),
				    static_cast<uint64_t>(sink), synthetic.deferred_nodes.size());
			// Destruction is deliberately outside the timed interval.  In wait
			// mode, hold it until perf has detached after the timed sample.
			if (!wait_mode)
				synthetic.deferred_nodes.clear();
		}
		if (wait_mode) {
			std::printf("timed-delete-complete pid=%d\n", getpid());
			std::fflush(stdout);
			wait_for_signal("after-timed-delete", true);
			synthetic.deferred_nodes.clear();
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

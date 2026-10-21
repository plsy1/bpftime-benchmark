/* SPDX-License-Identifier: MIT */
#ifndef BPFTIME_ARRAY_CPU_QUERY_HPP
#define BPFTIME_ARRAY_CPU_QUERY_HPP
#include <cstdint>

// Optional cold-path runtime/LLVM integration, not a BPF helper or mutable
// registry. The runtime resolves only its exact standard CPU implementation.
// JIT code must call read_cpu once per in-range lookup and invoke on_error
// exactly when it returns -1, matching the original standard helper.
struct bpftime_array_cpu_query_hooks {
	int (*read_cpu)() = nullptr;
	uint64_t (*on_error)() = nullptr;
};
extern "C" bool bpftime_resolve_array_cpu_query_v1(
	void *registered_helper, bpftime_array_cpu_query_hooks *hooks);
#endif

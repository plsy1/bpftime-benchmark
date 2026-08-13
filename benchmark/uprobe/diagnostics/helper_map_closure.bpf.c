// SPDX-License-Identifier: GPL-2.0
// Matched kernel/BPFtime ordinary/per-CPU array closure diagnostic.
// Kept separate from the hash closure so the production shared-memory size is
// not changed merely to fit every diagnostic map in one object.

#define BPF_NO_GLOBAL_DATA
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>

#define DEFINE_MAP(name, map_type)                                            \
	struct {                                                                 \
		__uint(type, map_type);                                           \
		__uint(max_entries, 1024);                                        \
		__type(key, u32);                                                  \
		__type(value, u64);                                                \
	} name SEC(".maps")

DEFINE_MAP(closure_array, BPF_MAP_TYPE_ARRAY);
DEFINE_MAP(closure_percpu_array, BPF_MAP_TYPE_PERCPU_ARRAY);

SEC("uprobe/benchmark/uprobe/.output/helper-map-closure/helper-map-closure-victim:__closure_setup")
int closure_setup(struct pt_regs *ctx)
{
#pragma clang loop unroll(disable)
	for (int i = 0; i < 1000; ++i) {
		u32 key = i;
		u64 value = i;
		bpf_map_update_elem(&closure_array, &key, &value, BPF_ANY);
		bpf_map_update_elem(&closure_percpu_array, &key, &value, BPF_ANY);
	}
	return 0;
}

SEC("uprobe/benchmark/uprobe/.output/helper-map-closure/helper-map-closure-victim:__closure_empty")
int closure_empty(struct pt_regs *ctx)
{
	return 0;
}

SEC("uprobe/benchmark/uprobe/.output/helper-map-closure/helper-map-closure-victim:__closure_lookup_control")
int closure_lookup_control(struct pt_regs *ctx)
{
#pragma clang loop unroll(disable)
	for (int i = 0; i < 1000; ++i) {
		u32 key = i;
		void *key_pointer = &key;
		barrier_var(key_pointer);
		barrier();
	}
	return 0;
}

SEC("uprobe/benchmark/uprobe/.output/helper-map-closure/helper-map-closure-victim:__closure_update_control")
int closure_update_control(struct pt_regs *ctx)
{
#pragma clang loop unroll(disable)
	for (int i = 0; i < 1000; ++i) {
		u32 key = i;
		u64 value = i;
		void *key_pointer = &key;
		void *value_pointer = &value;
		barrier_var(key_pointer);
		barrier_var(value_pointer);
		barrier();
	}
	return 0;
}

#define DEFINE_LOOKUP(program, map)                                           \
	SEC("uprobe/benchmark/uprobe/.output/helper-map-closure/helper-map-closure-victim:__closure_" #program) \
	int closure_##program(struct pt_regs *ctx)                              \
	{                                                                        \
	_Pragma("clang loop unroll(disable)")                                  \
		for (int i = 0; i < 1000; ++i) {                               \
			u32 key = i;                                              \
			bpf_map_lookup_elem(&map, &key);                           \
		}                                                                \
		return 0;                                                        \
	}

#define DEFINE_UPDATE(program, map)                                           \
	SEC("uprobe/benchmark/uprobe/.output/helper-map-closure/helper-map-closure-victim:__closure_" #program) \
	int closure_##program(struct pt_regs *ctx)                              \
	{                                                                        \
	_Pragma("clang loop unroll(disable)")                                  \
		for (int i = 0; i < 1000; ++i) {                               \
			u32 key = i;                                              \
			u64 value = i;                                            \
			bpf_map_update_elem(&map, &key, &value, BPF_ANY);          \
		}                                                                \
		return 0;                                                        \
	}

DEFINE_LOOKUP(array_lookup, closure_array)
DEFINE_UPDATE(array_update, closure_array)
DEFINE_LOOKUP(percpu_array_lookup, closure_percpu_array)
DEFINE_UPDATE(percpu_array_update, closure_percpu_array)

char LICENSE[] SEC("license") = "GPL";

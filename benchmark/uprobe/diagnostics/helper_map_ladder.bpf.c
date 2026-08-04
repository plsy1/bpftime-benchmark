// SPDX-License-Identifier: GPL-2.0
// Matched helper/array/hash lookup ladder.  No perf output is generated.

#define BPF_NO_GLOBAL_DATA
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1024);
	__type(key, u32);
	__type(value, u64);
} ladder_hash SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1024);
	__type(key, u32);
	__type(value, u64);
} ladder_array SEC(".maps");

SEC("uprobe/benchmark/uprobe/.output/helper-map-ladder/helper-map-ladder-victim:__ladder_setup")
int ladder_setup(struct pt_regs *ctx)
{
#pragma clang loop unroll(disable)
	for (int i = 0; i < 1000; ++i) {
		u32 key = i;
		u64 value = i;
		bpf_map_update_elem(&ladder_hash, &key, &value, BPF_ANY);
		bpf_map_update_elem(&ladder_array, &key, &value, BPF_ANY);
	}
	return 0;
}

SEC("uprobe/benchmark/uprobe/.output/helper-map-ladder/helper-map-ladder-victim:__ladder_empty")
int ladder_empty(struct pt_regs *ctx)
{
	return 0;
}

SEC("uprobe/benchmark/uprobe/.output/helper-map-ladder/helper-map-ladder-victim:__ladder_loop_control")
int ladder_loop_control(struct pt_regs *ctx)
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

SEC("uprobe/benchmark/uprobe/.output/helper-map-ladder/helper-map-ladder-victim:__ladder_simple_helper")
int ladder_simple_helper(struct pt_regs *ctx)
{
#pragma clang loop unroll(disable)
	for (int i = 0; i < 1000; ++i)
		bpf_get_current_pid_tgid();
	return 0;
}

SEC("uprobe/benchmark/uprobe/.output/helper-map-ladder/helper-map-ladder-victim:__ladder_array_lookup")
int ladder_array_lookup(struct pt_regs *ctx)
{
#pragma clang loop unroll(disable)
	for (int i = 0; i < 1000; ++i) {
		u32 key = i;
		bpf_map_lookup_elem(&ladder_array, &key);
	}
	return 0;
}

SEC("uprobe/benchmark/uprobe/.output/helper-map-ladder/helper-map-ladder-victim:__ladder_hash_hit")
int ladder_hash_hit(struct pt_regs *ctx)
{
#pragma clang loop unroll(disable)
	for (int i = 0; i < 1000; ++i) {
		u32 key = i;
		bpf_map_lookup_elem(&ladder_hash, &key);
	}
	return 0;
}

SEC("uprobe/benchmark/uprobe/.output/helper-map-ladder/helper-map-ladder-victim:__ladder_hash_miss")
int ladder_hash_miss(struct pt_regs *ctx)
{
#pragma clang loop unroll(disable)
	for (int i = 0; i < 1000; ++i) {
		u32 key = 100000 + i;
		bpf_map_lookup_elem(&ladder_hash, &key);
	}
	return 0;
}

char LICENSE[] SEC("license") = "GPL";

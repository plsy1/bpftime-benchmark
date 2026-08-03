// SPDX-License-Identifier: GPL-2.0
// Matched top-level empty/loop/full hash-lookup uprobe diagnostic.

#define BPF_NO_GLOBAL_DATA
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1024);
	__type(key, u32);
	__type(value, u64);
} thr_hash SEC(".maps");

SEC("uprobe/benchmark/uprobe/.output/top-hash-residual/top-hash-residual-victim:__thr_setup")
int thr_setup(struct pt_regs *ctx)
{
#pragma clang loop unroll(disable)
	for (int i = 0; i < 1000; ++i) {
		u32 key = i;
		u64 value = i;
		bpf_map_update_elem(&thr_hash, &key, &value, BPF_ANY);
	}
	return 0;
}

SEC("uprobe/benchmark/uprobe/.output/top-hash-residual/top-hash-residual-victim:__thr_empty")
int thr_empty(struct pt_regs *ctx)
{
	return 0;
}

SEC("uprobe/benchmark/uprobe/.output/top-hash-residual/top-hash-residual-victim:__thr_loop_control")
int thr_loop_control(struct pt_regs *ctx)
{
#pragma clang loop unroll(disable)
	for (int i = 0; i < 1000; ++i) {
		u32 key = i;
		void *key_pointer = &key;

		/* Keep the key/stack preparation without calling a helper. */
		barrier_var(key_pointer);
		barrier();
	}
	return 0;
}

SEC("uprobe/benchmark/uprobe/.output/top-hash-residual/top-hash-residual-victim:__thr_hash_lookup")
int thr_hash_lookup(struct pt_regs *ctx)
{
#pragma clang loop unroll(disable)
	for (int i = 0; i < 1000; ++i) {
		u32 key = i;
		bpf_map_lookup_elem(&thr_hash, &key);
	}
	return 0;
}

char LICENSE[] SEC("license") = "GPL";

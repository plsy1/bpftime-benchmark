// SPDX-License-Identifier: GPL-2.0
// Matched kernel-BPF control/real programs for ordinary array/hash map paths.

#define BPF_NO_GLOBAL_DATA
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1024);
	__type(key, u32);
	__type(value, u64);
} kmr_array SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1024);
	__type(key, u32);
	__type(value, u64);
} kmr_hash SEC(".maps");

static __always_inline void lookup_control_loop(void)
{
	for (int i = 0; i < 1000; ++i) {
		u32 key = i;
		void *key_pointer = &key;
		barrier_var(key_pointer);
		barrier();
	}
}

static __always_inline void update_control_loop(void)
{
	for (int i = 0; i < 1000; ++i) {
		u32 key = i;
		u64 value = i;
		void *key_pointer = &key;
		void *value_pointer = &value;
		barrier_var(key_pointer);
		barrier_var(value_pointer);
		barrier();
	}
}

SEC("uprobe")
int kmr_array_lookup_control(struct pt_regs *ctx)
{
	lookup_control_loop();
	return 0;
}

SEC("uprobe")
int kmr_array_lookup_real(struct pt_regs *ctx)
{
	for (int i = 0; i < 1000; ++i) {
		u32 key = i;
		bpf_map_lookup_elem(&kmr_array, &key);
	}
	return 0;
}

SEC("uprobe")
int kmr_array_update_control(struct pt_regs *ctx)
{
	update_control_loop();
	return 0;
}

SEC("uprobe")
int kmr_array_update_real(struct pt_regs *ctx)
{
	for (int i = 0; i < 1000; ++i) {
		u32 key = i;
		u64 value = i;
		bpf_map_update_elem(&kmr_array, &key, &value, BPF_ANY);
	}
	return 0;
}

SEC("uprobe")
int kmr_hash_lookup_control(struct pt_regs *ctx)
{
	lookup_control_loop();
	return 0;
}

SEC("uprobe")
int kmr_hash_lookup_real(struct pt_regs *ctx)
{
	for (int i = 0; i < 1000; ++i) {
		u32 key = i;
		bpf_map_lookup_elem(&kmr_hash, &key);
	}
	return 0;
}

SEC("uprobe")
int kmr_hash_update_control(struct pt_regs *ctx)
{
	update_control_loop();
	return 0;
}

SEC("uprobe")
int kmr_hash_update_real(struct pt_regs *ctx)
{
	for (int i = 0; i < 1000; ++i) {
		u32 key = i;
		u64 value = i;
		bpf_map_update_elem(&kmr_hash, &key, &value, BPF_ANY);
	}
	return 0;
}

char LICENSE[] SEC("license") = "GPL";

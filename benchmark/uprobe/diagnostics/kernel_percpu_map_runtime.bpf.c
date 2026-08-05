// SPDX-License-Identifier: GPL-2.0
// Matched kernel-BPF ordinary/per-CPU map runtime diagnostic.
//
// The control and real programs have the same loop, key/value preparation and
// compiler barriers.  The only intended difference is the map helper call.

#define BPF_NO_GLOBAL_DATA
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1024);
	__type(key, u32);
	__type(value, u64);
} kpmr_array SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(max_entries, 1024);
	__type(key, u32);
	__type(value, u64);
} kpmr_percpu_array SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1024);
	__type(key, u32);
	__type(value, u64);
} kpmr_hash SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_HASH);
	__uint(max_entries, 1024);
	__type(key, u32);
	__type(value, u64);
} kpmr_percpu_hash SEC(".maps");

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

#define DEFINE_LOOKUP_UPDATE(prefix, map_object)                              \
SEC("uprobe")                                                                  \
int prefix##_l_ctl(struct pt_regs *ctx)                                        \
{                                                                               \
	lookup_control_loop();                                                     \
	return 0;                                                                   \
}                                                                               \
SEC("uprobe")                                                                  \
int prefix##_l_real(struct pt_regs *ctx)                                       \
{                                                                               \
	for (int i = 0; i < 1000; ++i) {                                           \
		u32 key = i;                                                           \
		bpf_map_lookup_elem(&map_object, &key);                                \
	}                                                                           \
	return 0;                                                                   \
}                                                                               \
SEC("uprobe")                                                                  \
int prefix##_u_ctl(struct pt_regs *ctx)                                        \
{                                                                               \
	update_control_loop();                                                     \
	return 0;                                                                   \
}                                                                               \
SEC("uprobe")                                                                  \
int prefix##_u_real(struct pt_regs *ctx)                                       \
{                                                                               \
	for (int i = 0; i < 1000; ++i) {                                           \
		u32 key = i;                                                           \
		u64 value = i;                                                         \
		bpf_map_update_elem(&map_object, &key, &value, BPF_ANY);                \
	}                                                                           \
	return 0;                                                                   \
}

DEFINE_LOOKUP_UPDATE(kpa, kpmr_array)
DEFINE_LOOKUP_UPDATE(kpp, kpmr_percpu_array)
DEFINE_LOOKUP_UPDATE(kha, kpmr_hash)
DEFINE_LOOKUP_UPDATE(khp, kpmr_percpu_hash)

char LICENSE[] SEC("license") = "GPL";

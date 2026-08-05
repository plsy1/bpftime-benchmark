// SPDX-License-Identifier: GPL-2.0
// Kernel-BPF delete-hit diagnostic for ordinary/per-CPU hash maps.

#define BPF_NO_GLOBAL_DATA
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1024);
	__type(key, u32);
	__type(value, u64);
} khd_hash SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_HASH);
	__uint(max_entries, 1024);
	__type(key, u32);
	__type(value, u64);
} khd_percpu_hash SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1024);
	__uint(map_flags, BPF_F_NO_PREALLOC);
	__type(key, u32);
	__type(value, u64);
} khd_hash_noprealloc SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_HASH);
	__uint(max_entries, 1024);
	__uint(map_flags, BPF_F_NO_PREALLOC);
	__type(key, u32);
	__type(value, u64);
} khd_percpu_hash_noprealloc SEC(".maps");

static __always_inline void delete_control_loop(void)
{
	for (int i = 0; i < 1000; ++i) {
		u32 key = i;
		void *key_pointer = &key;
		barrier_var(key_pointer);
		barrier();
	}
}

#define DEFINE_DELETE(prefix, map_object)                                      \
SEC("uprobe")                                                                  \
int prefix##_ctl(struct pt_regs *ctx)                                           \
{                                                                                \
	delete_control_loop();                                                        \
	return 0;                                                                      \
}                                                                                \
SEC("uprobe")                                                                  \
int prefix##_real(struct pt_regs *ctx)                                          \
{                                                                                \
	for (int i = 0; i < 1000; ++i) {                                              \
		u32 key = i;                                                               \
		bpf_map_delete_elem(&map_object, &key);                                   \
	}                                                                            \
	return 0;                                                                      \
}

DEFINE_DELETE(khd_hash, khd_hash)
DEFINE_DELETE(khd_percpu_hash, khd_percpu_hash)
DEFINE_DELETE(khd_hash_noprealloc, khd_hash_noprealloc)
DEFINE_DELETE(khd_percpu_hash_noprealloc, khd_percpu_hash_noprealloc)

char LICENSE[] SEC("license") = "GPL";

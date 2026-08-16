// SPDX-License-Identifier: GPL-2.0
// Diagnostic-only matched ordinary/per-CPU hash workload sweep.

#define BPF_NO_GLOBAL_DATA
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>

#ifndef HWS_ACTIVE_KEYS
#define HWS_ACTIVE_KEYS 1000
#endif
#ifndef HWS_MAX_ENTRIES
#define HWS_MAX_ENTRIES 1024
#endif
#ifndef HWS_KEY_SIZE
#define HWS_KEY_SIZE 4
#endif
#ifndef HWS_VALUE_SIZE
#define HWS_VALUE_SIZE 8
#endif
#ifndef HWS_VICTIM_PATH
#define HWS_VICTIM_PATH "benchmark/uprobe/.output/hash-workload-sweep-baseline/hash-workload-sweep-victim"
#endif

#if HWS_KEY_SIZE == 4
struct hws_key { u32 id; };
#elif HWS_KEY_SIZE == 16
struct hws_key { u32 id; u8 padding[12]; };
#elif HWS_KEY_SIZE == 64
struct hws_key { u32 id; u8 padding[60]; };
#else
#error "HWS_KEY_SIZE must be 4, 16, or 64"
#endif

#if HWS_VALUE_SIZE == 8
struct hws_value { u64 id; };
#elif HWS_VALUE_SIZE == 64
struct hws_value { u64 id; u8 padding[56]; };
#elif HWS_VALUE_SIZE == 256
struct hws_value { u64 id; u8 padding[248]; };
#else
#error "HWS_VALUE_SIZE must be 8, 64, or 256"
#endif

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, HWS_MAX_ENTRIES);
	__type(key, struct hws_key);
	__type(value, struct hws_value);
} hws_hash SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_HASH);
	__uint(max_entries, HWS_MAX_ENTRIES);
	__type(key, struct hws_key);
	__type(value, struct hws_value);
} hws_percpu_hash SEC(".maps");

#define HWS_SEC(function) SEC("uprobe/" HWS_VICTIM_PATH ":" #function)

static __always_inline void init_key(struct hws_key *key, u32 id)
{
	__builtin_memset(key, 0, sizeof(*key));
	key->id = id;
}

static __always_inline void init_value(struct hws_value *value, u64 id)
{
	__builtin_memset(value, 0, sizeof(*value));
	value->id = id;
}

HWS_SEC(__hws_setup)
int hws_setup(struct pt_regs *ctx)
{
#pragma clang loop unroll(disable)
	for (int i = 0; i < HWS_ACTIVE_KEYS; ++i) {
		struct hws_key key;
		struct hws_value value;
		init_key(&key, i);
		init_value(&value, i);
		bpf_map_update_elem(&hws_hash, &key, &value, BPF_ANY);
		bpf_map_update_elem(&hws_percpu_hash, &key, &value, BPF_ANY);
	}
	return 0;
}

HWS_SEC(__hws_empty)
int hws_empty(struct pt_regs *ctx)
{
	return 0;
}

HWS_SEC(__hws_lookup_control)
int hws_lookup_control(struct pt_regs *ctx)
{
#pragma clang loop unroll(disable)
	for (int i = 0; i < 1000; ++i) {
		struct hws_key key;
		void *key_pointer;
		init_key(&key, i % HWS_ACTIVE_KEYS);
		key_pointer = &key;
		barrier_var(key_pointer);
		barrier();
	}
	return 0;
}

HWS_SEC(__hws_update_control)
int hws_update_control(struct pt_regs *ctx)
{
#pragma clang loop unroll(disable)
	for (int i = 0; i < 1000; ++i) {
		struct hws_key key;
		struct hws_value value;
		void *key_pointer;
		void *value_pointer;
		init_key(&key, i % HWS_ACTIVE_KEYS);
		init_value(&value, i);
		key_pointer = &key;
		value_pointer = &value;
		barrier_var(key_pointer);
		barrier_var(value_pointer);
		barrier();
	}
	return 0;
}

#define DEFINE_LOOKUP(program, map)                                         \
	HWS_SEC(__hws_##program)                                               \
	int hws_##program(struct pt_regs *ctx)                                 \
	{                                                                      \
	_Pragma("clang loop unroll(disable)")                                \
		for (int i = 0; i < 1000; ++i) {                             \
			struct hws_key key;                                      \
			init_key(&key, i % HWS_ACTIVE_KEYS);                     \
			bpf_map_lookup_elem(&map, &key);                         \
		}                                                              \
		return 0;                                                      \
	}

#define DEFINE_UPDATE(program, map)                                         \
	HWS_SEC(__hws_##program)                                               \
	int hws_##program(struct pt_regs *ctx)                                 \
	{                                                                      \
	_Pragma("clang loop unroll(disable)")                                \
		for (int i = 0; i < 1000; ++i) {                             \
			struct hws_key key;                                      \
			struct hws_value value;                                  \
			init_key(&key, i % HWS_ACTIVE_KEYS);                     \
			init_value(&value, i);                                   \
			bpf_map_update_elem(&map, &key, &value, BPF_ANY);        \
		}                                                              \
		return 0;                                                      \
	}

DEFINE_LOOKUP(hash_lookup, hws_hash)
DEFINE_UPDATE(hash_update, hws_hash)
DEFINE_LOOKUP(percpu_hash_lookup, hws_percpu_hash)
DEFINE_UPDATE(percpu_hash_update, hws_percpu_hash)

char LICENSE[] SEC("license") = "GPL";

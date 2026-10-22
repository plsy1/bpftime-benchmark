// SPDX-License-Identifier: GPL-2.0
// Diagnostic hash update/delete workload in three program shapes (hash only,
// hash + global data, hash + array result map). Experiment-only source.
#define BPF_NO_GLOBAL_DATA
#define BPF_NO_PRESERVE_ACCESS_INDEX
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#ifndef HASH_MUTATE_PATH
#error HASH_MUTATE_PATH must name the victim
#endif
#ifndef HASH_MUTATE_RESULT_MAP_TYPE
#define HASH_MUTATE_RESULT_MAP_TYPE BPF_MAP_TYPE_HASH
#endif
struct outcome { u64 seed; u64 checksum; u64 ok; u64 failed; };
struct { __uint(type, BPF_MAP_TYPE_HASH); __uint(max_entries, 1024);
    __type(key, u32); __type(value, u64); } data_map SEC(".maps");
struct { __uint(type, HASH_MUTATE_RESULT_MAP_TYPE); __uint(max_entries, 4);
    __type(key, u32); __type(value, struct outcome); } result_map SEC(".maps");
#ifdef HASH_MUTATE_GLOBALS
const volatile __u64 checksum_bias = 0;
__u64 invocations;
#define GLOBAL_BIAS checksum_bias
#define COUNT_INVOCATION() (++invocations)
#else
#define GLOBAL_BIAS 0
#define COUNT_INVOCATION() ((void)0)
#endif

enum { CONTROL, UPDATE, DELETE_INSERT };

// Keys 0..511 are present (loader). UPDATE rewrites a present key's value
// with BPF_EXIST; DELETE_INSERT deletes a present key and inserts it again
// with BPF_NOEXIST, so the table contents are the same after every call.
static __always_inline int run(struct pt_regs *ctx, u32 slot, int mode)
{
    u64 seed = PT_REGS_PARM1(ctx);
    u32 state = seed;
    u64 sum = GLOBAL_BIAS, ok = 0, failed = 0;
    COUNT_INVOCATION();
#pragma clang loop unroll(disable)
    for (u32 i = 0; i < 1000; ++i) {
        state = state * 1664525u + 1013904223u;
        u32 key = (state >> 16) & 511;
        u64 value = key * 17u + 5u;
        sum += key;
        if (mode == CONTROL) {
            ++ok;
        } else if (mode == UPDATE) {
            if (bpf_map_update_elem(&data_map, &key, &value, BPF_EXIST)) ++failed; else ++ok;
        } else {
            if (bpf_map_delete_elem(&data_map, &key)) ++failed; else ++ok;
            if (bpf_map_update_elem(&data_map, &key, &value, BPF_NOEXIST)) ++failed; else ++ok;
        }
    }
    struct outcome *out = bpf_map_lookup_elem(&result_map, &slot);
    if (out) {
        out->seed = seed; out->checksum = sum; out->ok = ok; out->failed = failed;
    }
    return sum;
}
SEC("uprobe/" HASH_MUTATE_PATH ":__hm_control") int control(struct pt_regs *ctx) { return run(ctx, 0, CONTROL); }
SEC("uprobe/" HASH_MUTATE_PATH ":__hm_update") int update(struct pt_regs *ctx) { return run(ctx, 1, UPDATE); }
SEC("uprobe/" HASH_MUTATE_PATH ":__hm_delete_insert") int delete_insert(struct pt_regs *ctx) { return run(ctx, 2, DELETE_INSERT); }
char LICENSE[] SEC("license") = "GPL";

// SPDX-License-Identifier: GPL-2.0
// Diagnostic hash lookup workload: separates key order, table fill, value use
// and miss cost. Experiment-only source (not part of the bpftime tree).
#define BPF_NO_GLOBAL_DATA
#define BPF_NO_PRESERVE_ACCESS_INDEX
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#ifndef HASH_SHAPE_FILL
#define HASH_SHAPE_FILL 1024
#endif
#ifndef HASH_SHAPE_PATH
#error HASH_SHAPE_PATH must name the victim
#endif
struct outcome { u64 seed; u64 checksum; u64 hits; u64 misses; };
struct { __uint(type, BPF_MAP_TYPE_HASH); __uint(max_entries, 1024);
    __type(key, u32); __type(value, u64); } data_map SEC(".maps");
// Hash as well, so the program uses a single map type.
struct { __uint(type, BPF_MAP_TYPE_HASH); __uint(max_entries, 8);
    __type(key, u32); __type(value, struct outcome); } result_map SEC(".maps");

enum { CONTROL, SEQ_UNUSED, SEQ, RAND_HIT, RAND_MISS };

static __always_inline int run(struct pt_regs *ctx, u32 slot, int mode)
{
    u64 seed = PT_REGS_PARM1(ctx);
    u32 state = seed;
    u64 sum = 0, hits = 0, misses = 0;
#pragma clang loop unroll(disable)
    for (u32 i = 0; i < 1000; ++i) {
        state = state * 1664525u + 1013904223u;
        u32 key;
        if (mode == SEQ_UNUSED || mode == SEQ)
            key = i < HASH_SHAPE_FILL ? i : i - HASH_SHAPE_FILL;
        else if (mode == RAND_MISS)
            key = (state >> 16) | 0x80000000u; // never inserted
        else
            key = (state >> 16) % HASH_SHAPE_FILL;
        if (mode == CONTROL) {
            sum += key * 17u + 5u;
            ++hits;
        } else if (mode == SEQ_UNUSED) {
            bpf_map_lookup_elem(&data_map, &key); // like the official benchmark
        } else {
            u64 *value = bpf_map_lookup_elem(&data_map, &key);
            if (value) { sum += *value; ++hits; }
            else { sum += 0x9e3779b9u; ++misses; }
        }
    }
    struct outcome *out = bpf_map_lookup_elem(&result_map, &slot);
    if (out) {
        out->seed = seed; out->checksum = sum; out->hits = hits; out->misses = misses;
    }
    return sum;
}
SEC("uprobe/" HASH_SHAPE_PATH ":__hs_control") int control(struct pt_regs *ctx) { return run(ctx, 0, CONTROL); }
SEC("uprobe/" HASH_SHAPE_PATH ":__hs_seq_unused") int seq_unused(struct pt_regs *ctx) { return run(ctx, 1, SEQ_UNUSED); }
SEC("uprobe/" HASH_SHAPE_PATH ":__hs_seq") int seq(struct pt_regs *ctx) { return run(ctx, 2, SEQ); }
SEC("uprobe/" HASH_SHAPE_PATH ":__hs_rand_hit") int rand_hit(struct pt_regs *ctx) { return run(ctx, 3, RAND_HIT); }
SEC("uprobe/" HASH_SHAPE_PATH ":__hs_rand_miss") int rand_miss(struct pt_regs *ctx) { return run(ctx, 4, RAND_MISS); }
char LICENSE[] SEC("license") = "GPL";

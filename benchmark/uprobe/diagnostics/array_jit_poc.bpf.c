// SPDX-License-Identifier: GPL-2.0
#define BPF_NO_GLOBAL_DATA
#define BPF_NO_PRESERVE_ACCESS_INDEX
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

struct outcome { u64 seed; u64 checksum; u64 hits; u64 misses; };
struct { __uint(type,BPF_MAP_TYPE_ARRAY); __uint(max_entries,1024);
    __type(key,u32); __type(value,u64); } data_map SEC(".maps");
struct { __uint(type,BPF_MAP_TYPE_ARRAY); __uint(max_entries,4);
    __type(key,u32); __type(value,struct outcome); } result_map SEC(".maps");

#define PATH "uprobe/benchmark/uprobe/.output/array-jit-poc/victim:"
static __always_inline int run(struct pt_regs *ctx, u32 slot, u32 mask, int control)
{
    u64 seed = PT_REGS_PARM1(ctx);
    u32 state = seed;
    u64 sum=0, hits=0, misses=0;
#pragma clang loop unroll(disable)
    for (u32 i=0; i<1000; ++i) {
        state = state * 1664525u + 1013904223u;
        u32 key = (state >> 16) & mask;
        if (control) {
            if (key < 1024) { sum += key * 17u + 5u; ++hits; }
            else { sum += 0x9e3779b9u; ++misses; }
        } else {
            u64 *value = bpf_map_lookup_elem(&data_map,&key);
            if (value) { sum += *value; ++hits; }
            else { sum += 0x9e3779b9u; ++misses; }
        }
    }
    struct outcome *out = bpf_map_lookup_elem(&result_map,&slot);
    if (out) {
        out->seed=seed; out->checksum=sum; out->hits=hits; out->misses=misses;
    }
    return sum;
}
SEC(PATH "__poc_control_hit") int control_hit(struct pt_regs *ctx) {return run(ctx,0,1023,1);}
SEC(PATH "__poc_hit") int hit(struct pt_regs *ctx) {return run(ctx,1,1023,0);}
SEC(PATH "__poc_control_mixed") int control_mixed(struct pt_regs *ctx) {return run(ctx,2,2047,1);}
SEC(PATH "__poc_mixed") int mixed(struct pt_regs *ctx) {return run(ctx,3,2047,0);}
// Demonstration only: this matches the original unused-result workload and
// is expected to disappear under successful JIT inlining. NOT a gain metric.
SEC(PATH "__poc_unused") int unused(struct pt_regs *ctx)
{
#pragma clang loop unroll(disable)
    for(u32 i=0;i<1000;++i) {u32 key=i; bpf_map_lookup_elem(&data_map,&key);}
    return 0;
}
char LICENSE[] SEC("license")="GPL";

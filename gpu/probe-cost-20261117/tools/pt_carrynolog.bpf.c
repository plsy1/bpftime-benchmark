/* carryonly without the log2 (attribution, 11-17): as pt_lat.bpf.c with
 * PT_NOMERGE, but the exit compares the raw delta with a constant no run
 * reaches (2^63 - 1 ns) instead of computing the bucket. carryonly minus
 * this is the log2 chain. */
#include "pt_common.h"
struct { __uint(type, MT_THREAD_LOCAL); __uint(max_entries, 1); __type(key, u32); __type(value, u64); } start SEC(".maps");
struct { __uint(type, MT_SUM); __uint(max_entries, 65 * NK); __type(key, u32); __type(value, u64); } hist SEC(".maps");
static __always_inline int on_entry(u32 k)
{
	u32 z = 0;
	u64 *s = bpf_map_lookup_elem(&start, &z);
	if (s)
		*s = bpf_get_globaltimer();
	return 0;
}
static __always_inline int on_exit(u32 k)
{
	u32 z = 0, key;
	u64 *s = bpf_map_lookup_elem(&start, &z);
	if (!s || *s == 0)
		return 0;
	if (bpf_get_globaltimer() - *s != 0x7fffffffffffffffULL)
		return 0;
	key = k * 65 + 63;
	u64 *c = bpf_map_lookup_elem(&hist, &key);
	if (c)
		__sync_fetch_and_add(c, 1);
	return 0;
}
#define DEF(fn, kidx, kname) \
	SEC("kprobe/" kname) int cuda__in_##fn() { return on_entry(kidx); } \
	SEC("kretprobe/" kname) int cuda__out_##fn() { return on_exit(kidx); }
PT_PAIRS(DEF)
char LICENSE[] SEC("license") = "GPL";

/* The count alone (attribution, 11-17): an empty entry probe; the exit adds 1
 * to word k*65 of a sum map (1531) with a constant key, no timer, no carry.
 * The same lookup helper and atomic as lat's merge, every thread in one
 * bucket. */
#include "pt_common.h"
struct { __uint(type, MT_SUM); __uint(max_entries, 65 * NK); __type(key, u32); __type(value, u64); } hist SEC(".maps");
static __always_inline int on_exit(u32 k)
{
	u32 key = k * 65;
	u64 *c = bpf_map_lookup_elem(&hist, &key);
	if (c)
		__sync_fetch_and_add(c, 1);
	return 0;
}
#define DEF(fn, kidx, kname) \
	SEC("kprobe/" kname) int cuda__in_##fn() { return 0; } \
	SEC("kretprobe/" kname) int cuda__out_##fn() { return on_exit(kidx); }
PT_PAIRS(DEF)
char LICENSE[] SEC("license") = "GPL";

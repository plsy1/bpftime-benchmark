/* threadhist: exits per thread index (one counter per key k per thread),
 *   PT_OLD: today's 1502 (-> 1512), `*cnt += 1` as the example;
 *   else: 1532 (device memory, delivered), an atomic add. */
#include "pt_common.h"
#ifdef PT_OLD
struct { __uint(type, MT_PERTHREAD); __uint(max_entries, NK); __type(key, u32); __type(value, u64); } calls SEC(".maps");
#else
struct { __uint(type, MT_DEV_PERTHREAD); __uint(max_entries, NK); __type(key, u32); __type(value, u64); } calls SEC(".maps");
#endif
static __always_inline int on_exit(u32 k)
{
	u64 *c = bpf_map_lookup_elem(&calls, &k);
	if (c) {
#ifdef PT_OLD
		*c += 1;
#else
		__sync_fetch_and_add(c, 1);
#endif
	}
	return 0;
}
#define DEF(fn, kidx, kname) \
	SEC("kretprobe/" kname) int cuda__out_##fn() { return on_exit(kidx); }
PT_PAIRS(DEF)
char LICENSE[] SEC("license") = "GPL";

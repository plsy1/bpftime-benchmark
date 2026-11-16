/* Atomics on thread-local (1530) values: the routing must turn them into
 * plain read-modify-writes (a generic atom on a local address is undefined).
 * Entry: 64-bit fetch-add 5 (result used), 32-bit add 3, exchange 7 on
 * three values of the thread's copy. Exit: they must read 5, 3 and 7.
 * chk (1531), per kernel k: k*8 + 0 exits, 1 mismatches, 2 NULL lookups,
 * 3 fetch-add results not 0. */
#include "pt_common.h"
struct { __uint(type, MT_THREAD_LOCAL); __uint(max_entries, 3); __type(key, u32); __type(value, u64); } tl SEC(".maps");
struct { __uint(type, MT_SUM); __uint(max_entries, 8 * NK); __type(key, u32); __type(value, u64); } chk SEC(".maps");
static __always_inline void count(u32 key)
{
	u64 *c = bpf_map_lookup_elem(&chk, &key);
	if (c)
		__sync_fetch_and_add(c, 1);
}
static __always_inline int on_entry(u32 k)
{
	u32 k0 = 0, k1 = 1, k2 = 2;
	u64 *a = bpf_map_lookup_elem(&tl, &k0);
	u64 *b = bpf_map_lookup_elem(&tl, &k1);
	u64 *c = bpf_map_lookup_elem(&tl, &k2);
	if (!a || !b || !c) { count(k * 8 + 2); return 0; }
	if (__sync_fetch_and_add(a, 5) != 0)
		count(k * 8 + 3);
	__sync_fetch_and_add((u32 *)b, 3);
	__sync_lock_test_and_set(c, 7);
	return 0;
}
static __always_inline int on_exit(u32 k)
{
	u32 k0 = 0, k1 = 1, k2 = 2;
	u64 *a = bpf_map_lookup_elem(&tl, &k0);
	u64 *b = bpf_map_lookup_elem(&tl, &k1);
	u64 *c = bpf_map_lookup_elem(&tl, &k2);
	if (!a || !b || !c) { count(k * 8 + 2); return 0; }
	if (*a != 5 || *b != 3 || *c != 7)
		count(k * 8 + 1);
	count(k * 8 + 0);
	return 0;
}
#define DEF(fn, kidx, kname) \
	SEC("kprobe/" kname) int cuda__in_##fn() { return on_entry(kidx); } \
	SEC("kretprobe/" kname) int cuda__out_##fn() { return on_exit(kidx); }
PT_PAIRS(DEF)
char LICENSE[] SEC("license") = "GPL";

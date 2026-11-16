/* Per-thread duration from the kernel's entry to the thread's exit, as a
 * log2 histogram per kernel.
 *   PT_OLD undefined (the model): start in a thread-local map (1530), the
 *     histogram in a sum map (1531);
 *   PT_OLD (today): start in a per-thread array (1502 -> 1512, indexed by
 *     thread), the histogram in a GPU array (1503 -> 1513, registered host
 *     memory);
 *   PT_MIX: today's start (1512), the model's histogram (1531);
 *   PT_NOMERGE: the model's start, no histogram add (the carry alone).
 * hist: key k*65 + b (b < 64: log2 of ns), k*65 + 64: exits without a start
 * (the start map gave NULL or 0). Every change is a 64-bit atomic add. */
#include "pt_common.h"
#if defined(PT_OLD)
struct { __uint(type, MT_PERTHREAD); __uint(max_entries, 1); __type(key, u32); __type(value, u64); } start SEC(".maps");
struct { __uint(type, MT_GPU_ARRAY); __uint(max_entries, 65 * NK); __type(key, u32); __type(value, u64); } hist SEC(".maps");
#elif defined(PT_MIX)
/* today's start, the model's histogram */
struct { __uint(type, MT_PERTHREAD); __uint(max_entries, 1); __type(key, u32); __type(value, u64); } start SEC(".maps");
struct { __uint(type, MT_SUM); __uint(max_entries, 65 * NK); __type(key, u32); __type(value, u64); } hist SEC(".maps");
#else
struct { __uint(type, MT_THREAD_LOCAL); __uint(max_entries, 1); __type(key, u32); __type(value, u64); } start SEC(".maps");
struct { __uint(type, MT_SUM); __uint(max_entries, 65 * NK); __type(key, u32); __type(value, u64); } hist SEC(".maps");
#endif
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
	if (!s || *s == 0) {
		key = k * 65 + 64;
	} else {
		key = k * 65 + log2_u64(bpf_get_globaltimer() - *s);
#if defined(PT_OLD) || defined(PT_MIX)
		*s = 0; /* the slot outlives the thread */
#endif
	}
#ifdef PT_NOMERGE
	/* the carry only: the bucket is computed and consumed by a branch
	 * that never adds (no exit lasts 2^63 ns) */
	if (key != k * 65 + 63)
		return 0;
#endif
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

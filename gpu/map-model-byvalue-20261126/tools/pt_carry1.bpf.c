/* Check of the by-value thread-local area (2026-11-26): one 8-byte value, so
 * the routing passes the area by value; the entry stores a 64-bit mark of the
 * thread ((thread index ^ salt) + 1, both halves vary), the exit compares
 * every bit. chk (1531), per kernel k: k*8 + 0 entries, 1 exits,
 * 2 mismatches, 3 exits without an entry mark, 4 NULL lookups. */
#include "pt_common.h"
#define SALT 0x9e3779b97f4a7c15ull
struct { __uint(type, MT_THREAD_LOCAL); __uint(max_entries, 1); __type(key, u32); __type(value, u64); } carry SEC(".maps");
struct { __uint(type, MT_SUM); __uint(max_entries, 8 * NK); __type(key, u32); __type(value, u64); } chk SEC(".maps");

static __always_inline void count(u32 key)
{
	u64 *c = bpf_map_lookup_elem(&chk, &key);
	if (c)
		__sync_fetch_and_add(c, 1);
}
static __always_inline int on_entry(u32 k)
{
	u32 k0 = 0;
	u64 *a = bpf_map_lookup_elem(&carry, &k0);
	if (!a) { count(k * 8 + 4); return 0; }
	*a = (lin_tid() ^ (SALT + k)) + 1;
	count(k * 8 + 0);
	return 0;
}
static __always_inline int on_exit(u32 k)
{
	u32 k0 = 0;
	u64 *a = bpf_map_lookup_elem(&carry, &k0);
	if (!a) { count(k * 8 + 4); return 0; }
	if (*a == 0)
		count(k * 8 + 3);
	else if (*a != (lin_tid() ^ (SALT + k)) + 1)
		count(k * 8 + 2);
	count(k * 8 + 1);
	return 0;
}
#define DEF(fn, kidx, kname) \
	SEC("kprobe/" kname) int cuda__in_##fn() { return on_entry(kidx); } \
	SEC("kretprobe/" kname) int cuda__out_##fn() { return on_exit(kidx); }
PT_PAIRS(DEF)
char LICENSE[] SEC("license") = "GPL";

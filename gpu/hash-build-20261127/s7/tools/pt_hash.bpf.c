/* Hash maps from GPU probes, as they are today (2026-11-26, "measure the
 * current state"). Only today's map types: h is an ordinary
 * BPF_MAP_TYPE_HASH (every GPU operation on it is a host call), chk is a
 * GPU array (1503, created as 1513: registered host memory, adds made on the
 * device) for side counts. No 1530/1531/1532 map, so the routing pass does
 * not run on these programs.
 *
 * One operation per build (-DHS_OP=...), at one rate (-DHS_RATE=...), with
 * one key shape (-DHS_KEY=...), in the exit probe; HS_OP_TID also uses the
 * entry probe.
 *   ops:
 *     CNT   the counter idiom of the examples: lookup; on a hit an atomic
 *           add of 1, on a miss an insert of 1 with BPF_NOEXIST (a failed
 *           insert is counted in chk and the increment is lost, as in the
 *           examples)
 *     PRE   lookup + atomic add of 1, keys inserted by the loader (no
 *           first touch); a miss is counted in chk
 *     MISS  lookup of a key never inserted (a hit is counted in chk)
 *     UPD   update with BPF_ANY of a key the loader inserted (the value
 *           moves to a new element on every update)
 *     DEL   delete of a key never inserted (a success is counted in chk)
 *     TID   entry: update(thread key, now, BPF_ANY); exit: lookup (found
 *           or missing counted in chk), then delete (failures in chk)
 *     TIDND TID without the delete
 *     NONE  the rate filter and the probe-run count only, no hash operation
 *           (the zero point of a rate, 2026-11-26 addendum)
 *     RMW   lookup; atomic add of 1 through the pointer; then update with
 *           BPF_EXIST passing that pointer as the value (cuda-counter's
 *           total_time_ns pattern); a miss inserts 1 with BPF_NOEXIST
 *   rates: THREAD every thread; BLOCK the thread (0,0,0) of each block;
 *          LEADER thread (0,0,0) of block (0,0,0) of each launch
 *   keys:  KC k (one per kernel); KSM 16 + 64 k + %smid; KBLK
 *          4096 + 4096 k + blockIdx.x; TID uses 131072 k + linear thread
 * chk words per kernel k: k*8 + 0 found/hit, 1 missing/NULL, 2 delete
 * failed (or a delete that succeeded, DEL), 3 insert or update failed,
 * 4 probe runs (every thread that passed the rate filter).
 *
 * -DHS_DEV (2026-11-28, stage one built): h is the device hash map (1533)
 * instead, with its companion status array h_st (the hash's name cut to 12
 * bytes plus "_st": an ARRAY of at least 16 u64 words the owner's delivery
 * writes). Only CNT, PRE and MISS: 1533 takes NOEXIST inserts and 64-bit
 * adds through lookups; updates with other flags and deletes return -22.
 * HS_MAX sets h's max_entries (default 262144). */
#include "pt_common.h"

#define HS_OP_CNT 1
#define HS_OP_PRE 2
#define HS_OP_MISS 3
#define HS_OP_UPD 4
#define HS_OP_DEL 5
#define HS_OP_TID 6
#define HS_OP_TIDND 7
#define HS_OP_RMW 8
#define HS_OP_NONE 9
#define HS_RATE_THREAD 1
#define HS_RATE_BLOCK 2
#define HS_RATE_LEADER 3
#define HS_KEY_KC 1
#define HS_KEY_KSM 2
#define HS_KEY_KBLK 3

#if !defined(HS_OP) || !defined(HS_RATE) || !defined(HS_KEY)
#error "HS_OP, HS_RATE and HS_KEY"
#endif

static const u64 (*bpf_get_sm_id)(void) = (void *)509;

#ifndef HS_MAX
#define HS_MAX 262144
#endif
#if defined(HS_DEV)
#if HS_OP != HS_OP_CNT && HS_OP != HS_OP_PRE && HS_OP != HS_OP_MISS
#error "HS_DEV: CNT, PRE and MISS only"
#endif
struct { __uint(type, MT_DEV_HASH); __uint(max_entries, HS_MAX); __type(key, u32); __type(value, u64); } h SEC(".maps");
struct { __uint(type, BPF_MAP_TYPE_ARRAY); __uint(max_entries, 16); __type(key, u32); __type(value, u64); } h_st SEC(".maps");
#else
struct { __uint(type, BPF_MAP_TYPE_HASH); __uint(max_entries, HS_MAX); __type(key, u32); __type(value, u64); } h SEC(".maps");
#endif
struct { __uint(type, MT_GPU_ARRAY); __uint(max_entries, 8 * NK); __type(key, u32); __type(value, u64); } chk SEC(".maps");

static __always_inline void count(u32 key)
{
	u64 *c = bpf_map_lookup_elem(&chk, &key);
	if (c)
		__sync_fetch_and_add(c, 1);
}

static __always_inline int selected(void)
{
	u64 tx, ty, tz, bx, by, bz;
	if (HS_RATE == HS_RATE_THREAD)
		return 1;
	bpf_get_thread_idx(&tx, &ty, &tz);
	if (tx | ty | tz)
		return 0;
	if (HS_RATE == HS_RATE_BLOCK)
		return 1;
	bpf_get_block_idx(&bx, &by, &bz);
	return !(bx | by | bz);
}

static __always_inline u32 key_of(u32 k)
{
	if (HS_KEY == HS_KEY_KSM)
		return 16 + 64 * k + (u32)bpf_get_sm_id();
	if (HS_KEY == HS_KEY_KBLK) {
		u64 bx, by, bz;
		bpf_get_block_idx(&bx, &by, &bz);
		return 4096 + 4096 * k + (u32)bx;
	}
	return k;
}

static __always_inline u32 tid_key(u32 k)
{
	return 131072 * k + (u32)lin_tid();
}

static __always_inline int on_entry(u32 k)
{
#if HS_OP == HS_OP_TID || HS_OP == HS_OP_TIDND
	if (!selected())
		return 0;
	u32 key = tid_key(k);
	u64 now = bpf_get_globaltimer();
	if (bpf_map_update_elem(&h, &key, &now, BPF_ANY))
		count(k * 8 + 3);
#endif
	return 0;
}

static __always_inline int on_exit(u32 k)
{
	if (!selected())
		return 0;
	count(k * 8 + 4);
#if HS_OP == HS_OP_TID || HS_OP == HS_OP_TIDND
	u32 key = tid_key(k);
	u64 *s = bpf_map_lookup_elem(&h, &key);
	if (s && *s)
		count(k * 8 + 0);
	else
		count(k * 8 + 1);
#if HS_OP == HS_OP_TID
	if (bpf_map_delete_elem(&h, &key))
		count(k * 8 + 2);
#endif
#else
	u32 key = key_of(k);
#if HS_OP == HS_OP_CNT || HS_OP == HS_OP_PRE || HS_OP == HS_OP_RMW
	u64 *v = bpf_map_lookup_elem(&h, &key);
	if (v) {
		__sync_fetch_and_add(v, 1);
		count(k * 8 + 0);
#if HS_OP == HS_OP_RMW
		if (bpf_map_update_elem(&h, &key, v, BPF_EXIST))
			count(k * 8 + 3);
#endif
	} else {
		count(k * 8 + 1);
#if HS_OP != HS_OP_PRE
		u64 one = 1;
		if (bpf_map_update_elem(&h, &key, &one, BPF_NOEXIST))
			count(k * 8 + 3);
#endif
	}
#elif HS_OP == HS_OP_MISS
	u32 absent = 200000 + key;
	if (bpf_map_lookup_elem(&h, &absent))
		count(k * 8 + 0);
	else
		count(k * 8 + 1);
#elif HS_OP == HS_OP_UPD
	u64 one = 1;
	if (bpf_map_update_elem(&h, &key, &one, BPF_ANY))
		count(k * 8 + 3);
#elif HS_OP == HS_OP_NONE
	(void)key;
#elif HS_OP == HS_OP_DEL
	u32 absent = 200000 + key;
	if (bpf_map_delete_elem(&h, &absent) == 0)
		count(k * 8 + 2);
#else
#error "unknown HS_OP"
#endif
#endif
	return 0;
}

#define DEF(fn, kidx, kname) \
	SEC("kprobe/" kname) int cuda__in_##fn() { return on_entry(kidx); } \
	SEC("kretprobe/" kname) int cuda__out_##fn() { return on_exit(kidx); }
PT_PAIRS(DEF)
char LICENSE[] SEC("license") = "GPL";

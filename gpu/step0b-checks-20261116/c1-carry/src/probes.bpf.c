// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
// probes.bpf.c: the eBPF programs of check C1, one source for every variant.
//
// PROG_CHK (part b, correctness), compiled once per kernel family with its
// own SALT:
//   entry: c = store_lookup(&store, 0, op=0); write (id ^ SALT, globaltimer)
//          into c AND into an independent ground-truth copy (rec key 0, a
//          per-thread array in device memory indexed by the thread's id);
//          count the entry.
//   exit:  c = store_lookup(&store, 0, op=1); compare c with the thread's id
//          and with the ground truth; count the exit, mismatches and NULLs;
//          record the values and the address c; then discard (zero) both.
// PROG_COST (part c): entry writes the timer into c, exit reads it and counts
//   a mismatch (a zero) or a NULL in a device counter; nothing else.
// PROG_VALUE (part c): the entry calls the timer helper and stores nothing.
// PROG_EMPTY (part c): both probes return at once.
//
// The storage is chosen at run time by map_info[store fd].map_type (the
// wrapper _bpf_helper_ext_0001_c1 dispatches on it) or, in the direct form,
// by the routing at build time; the eBPF is the same for every variant.
#define BPF_NO_GLOBAL_DATA
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>

struct cv {
	u64 a, b;
};
struct rec {
	u64 w[8];
};
// map types are set by the host (map_info); these are placeholders
struct {
	__uint(type, 1540);
	__uint(max_entries, 1);
	__type(key, u32);
	__type(value, struct cv);
} store SEC(".maps");
struct {
	__uint(type, 1502);
	__uint(max_entries, 2);
	__type(key, u32);
	__type(value, struct rec);
} rec SEC(".maps");
struct {
	__uint(type, 1503);
	__uint(max_entries, 8);
	__type(key, u32);
	__type(value, u64);
} cnt SEC(".maps");

// helper 1 with a third argument (r3 = op); existing lookups ignore r3
static void *(*store_lookup)(void *map, const void *key, u64 op) = (void *)1;
static u64 (*gtimer)(void) = (void *)502;
static long (*get_block_idx)(u64 *x, u64 *y, u64 *z) = (void *)503;
static long (*get_block_dim)(u64 *x, u64 *y, u64 *z) = (void *)504;
static long (*get_thread_idx)(u64 *x, u64 *y, u64 *z) = (void *)505;
static long (*get_grid_dim)(u64 *x, u64 *y, u64 *z) = (void *)508;

// cnt words
#define CNT_REC_NULL 0
#define CNT_NULL 1
#define CNT_MISMATCH 2

static __always_inline void cnt_add(u32 k)
{
	u64 *w = bpf_map_lookup_elem(&cnt, &k);
	if (w)
		__sync_fetch_and_add(w, 1);
}

#if defined(PROG_CHK)
#ifndef SALT
#error "SALT"
#endif
// the thread's linear id in its grid (all three dimensions)
static __always_inline u64 tuid(void)
{
	u64 bx, by, bz, dx, dy, dz, tx, ty, tz, gx, gy, gz;
	get_block_idx(&bx, &by, &bz);
	get_block_dim(&dx, &dy, &dz);
	get_thread_idx(&tx, &ty, &tz);
	get_grid_dim(&gx, &gy, &gz);
	return ((bz * gy + by) * gx + bx) * (dx * dy * dz) + (tz * dy + ty) * dx + tx;
}

SEC("kprobe/c1")
int cuda__c1_entry(void *ctx)
{
	const u64 id = tuid() ^ SALT; // SALT has bit 63 set: never 0
	const u64 t = gtimer();
	u32 k0 = 0;
	struct rec *r = bpf_map_lookup_elem(&rec, &k0);
	if (!r) {
		cnt_add(CNT_REC_NULL);
		return 0;
	}
	r->w[2] += 1; // entries
	struct cv *c = store_lookup(&store, &k0, 0);
	if (!c) {
		r->w[3] += 1; // no storage at entry
		return 0;
	}
	c->a = id;
	c->b = t;
	r->w[0] = id;
	r->w[1] = t;
	return 0;
}

SEC("kretprobe/c1")
int cuda__c1_exit(void *ctx)
{
	const u64 id = tuid() ^ SALT;
	const u64 now = gtimer();
	u32 k0 = 0, k1 = 1;
	struct rec *r = bpf_map_lookup_elem(&rec, &k0);
	struct rec *o = bpf_map_lookup_elem(&rec, &k1);
	if (!r || !o) {
		cnt_add(CNT_REC_NULL);
		return 0;
	}
	o->w[4] += 1; // exits
	struct cv *c = store_lookup(&store, &k0, 1);
	if (!c) {
		o->w[6] += 1; // no storage at exit (the slot design's fallback)
		r->w[0] = 0;
		r->w[1] = 0;
		return 0;
	}
	const u64 a = c->a, b = c->b, ra = r->w[0], rb = r->w[1];
	u64 bad = 0;
	if (a != id)
		bad |= 1; // not this thread's value
	if (b != rb)
		bad |= 2; // not this launch's entry (ground truth)
	if (ra != id)
		bad |= 4; // ground truth itself missing or foreign
	if (b > now)
		bad |= 8;
	o->w[0] = a;
	o->w[1] = b;
	o->w[2] = now;
	o->w[3] = bad;
	if (bad)
		o->w[5] += 1; // mismatches
	o->w[7] = (u64)c; // the address the probe used
	c->a = 0; // discard: a later read of stale state cannot match
	c->b = 0;
	r->w[0] = 0;
	r->w[1] = 0;
	return 0;
}

#elif defined(PROG_COST)
SEC("kprobe/c1")
int cuda__c1_entry(void *ctx)
{
	const u64 t = gtimer();
	u32 k0 = 0;
	u64 *c = store_lookup(&store, &k0, 0);
	if (!c) {
		cnt_add(CNT_NULL);
		return 0;
	}
#ifdef COST_BAD
	*c = 0; // sabotaged twin: every exit must count a mismatch
	(void)t;
#else
	*c = t;
#endif
	return 0;
}

SEC("kretprobe/c1")
int cuda__c1_exit(void *ctx)
{
	u32 k0 = 0;
	u64 *c = store_lookup(&store, &k0, 1);
	if (!c) {
		cnt_add(CNT_NULL);
		return 0;
	}
	if (*c == 0)
		cnt_add(CNT_MISMATCH);
	return 0;
}

#elif defined(PROG_VALUE)
// part (c): the entry computes the value (one helper call) and stores it
// nowhere; the exit does nothing. Subtracting this from (1)-(3) leaves the
// storage path alone.
SEC("kprobe/c1")
int cuda__c1_entry(void *ctx)
{
	gtimer();
	return 0;
}
SEC("kretprobe/c1")
int cuda__c1_exit(void *ctx)
{
	return 0;
}

#elif defined(PROG_EMPTY)
SEC("kprobe/c1")
int cuda__c1_entry(void *ctx)
{
	return 0;
}
SEC("kretprobe/c1")
int cuda__c1_exit(void *ctx)
{
	return 0;
}
#else
#error "define PROG_CHK, PROG_COST, PROG_VALUE or PROG_EMPTY"
#endif

char LICENSE[] SEC("license") = "GPL";

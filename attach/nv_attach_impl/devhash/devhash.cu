// The device hash map's GPU functions (BPF_MAP_TYPE_GPU_DEVICE_HASH_MAP, 1533;
// plan results gpu/hash-build-plan-20261127, step S3). A port of round two's
// tested protocol (results gpu/hash-round2-20261127, c5/src/c5.cu, protocol
// v2: claim the slot, then reserve a row), on the block layout of
// ../trampoline/gpu_devhash_layout.h.
//
// Shipped as ../devhash_ptx.h (make -C attach/nv_attach_impl/devhash): the
// routing pass puts this text at the start of a patched module that calls
// these functions; it is never part of the shared trampoline, which stays
// sm_61-legal. Three functions, u64 parameters and results:
//   __bpftime_devhash_lookup(base, key, max_entries, shards)
//     the calling thread's shard of K's value (a generic address), or 0:
//     K absent, its claim in flight or refused, or no device copy (base 0)
//   __bpftime_devhash_insert(base, key, value_ptr, flags, max_entries, shards)
//     BPF_NOEXIST only: 0 inserted (the value added into the caller's
//     shard), -17 EEXIST, -7 E2BIG, -22 other flags, -1 no device copy
//   __bpftime_devhash_layout_fp()
//     the layout fingerprint this text was built with
//
// Insert (v2): a thread that finds an empty slot reads the reservation count
// with ld.acquire; at max_entries it reads the slot again and returns E2BIG
// only if it is still empty. Otherwise it CASes the slot from empty to
// claiming(K). The winner adds 1 to the count (atom.acq_rel): an old value
// below max_entries is its row (add the value, publish live with
// st.release), otherwise it publishes refused(K) (permanent) and returns
// E2BIG. A thread that meets claiming(K) waits, bounded, for live (EEXIST)
// or refused (E2BIG); past the bound it returns EEXIST, counted unstable. A
// walk that passes every slot waits, bounded, for the count to reach
// max_entries (E2BIG; past the bound E2BIG counted unstable). Walks decide
// on a slot's state first: empty ends a walk, opaque (4) and others' keys
// are walked past. Every header load is ld.relaxed.gpu (strong); every
// access is on the .global address (cvta.to.global of the block).
//
// Test builds (BPFTIME_DEVHASH_TEST): the same three signatures; each insert
// also stores its outcome code at control word CTL_TEST_OUTCOMES's address
// plus the thread's global index (when that word is set), and counts the
// protocol's events in control words from CTL_TEST_DIAG. Controls
// (BPFTIME_DEVHASH_FAULT): 1 a plain store instead of the claim CAS; 2 no
// count check by the winner; 5 the precheck returns E2BIG without reading
// the slot again; 6 round one's order (reserve before the CAS, give back
// on a loss, rows from a separate counter). BPFTIME_DEVHASH_WAIT_BOUND sets
// the wait bound (requested nanosleep, ns).
#include <__clang_cuda_builtin_vars.h>
#include <cstdint>
#define __device__ __attribute__((device))
#define __global__ __attribute__((global))
#define __forceinline__ __inline__ __attribute__((always_inline))
#define __noinline__ __attribute__((noinline))
#include "../trampoline/gpu_devhash_layout.h"

#ifndef BPFTIME_DEVHASH_WAIT_BOUND
#define BPFTIME_DEVHASH_WAIT_BOUND 100000u
#endif
#ifndef BPFTIME_DEVHASH_FAULT
#define BPFTIME_DEVHASH_FAULT 0
#endif

namespace devhash_impl
{
using namespace bpftime_devhash;
typedef uint64_t u64;
typedef uint32_t u32;

constexpr u64 NOEXIST = 1; // BPF_NOEXIST
constexpr u64 R_EEXIST = (u64)-17;
constexpr u64 R_E2BIG = (u64)-7;
constexpr u64 R_EINVAL = (u64)-22;
constexpr u64 R_NO_COPY = (u64)-1;

// outcome codes (test builds), as round two's
enum : u32 {
	OC_NONE = 0,
	OC_HIT = 1,
	OC_WON = 2,
	OC_EEXIST_LIVE = 3,
	OC_EEXIST_WAITED = 4,
	OC_EEXIST_UNSTABLE = 5,
	OC_E2BIG = 6,
	OC_ANOM_ROW = 8,
	OC_EINVAL = 9,
	OC_FLAG_THEN_NULL = 0x10,
};
// diagnostic counters (test builds): control word CTL_TEST_DIAG + D_*
enum : u32 {
	D_KEY_CHANGED = 0,
	D_ROW_OVERFLOW,
	D_REFUSED_PUBLISHED,
	D_NEXT_ROW,
	D_GIVEBACK,
	D_LOST_SAME,
	D_LOST_OTHER,
	D_WAITS,
	D_E2BIG_PRECHECK,
	D_PRECHECK_SLOT_TAKEN,
	D_E2BIG_SAW_REFUSED,
	D_E2BIG_AFTER_ADD,
	D_E2BIG_FULLDIR,
	D_WAIT_MAX_NS,
	D_WALK_SAW_CLAIMING,
	D_N
};
static_assert(CTL_TEST_DIAG + D_N <= CTL_WORDS, "diagnostics fit");

// ----------------------------------------------------------- PTX forms
static __device__ __forceinline__ u64 to_global(u64 p)
{
	u64 g;
	asm("cvta.to.global.u64 %0, %1;" : "=l"(g) : "l"(p));
	return g;
}
static __device__ __forceinline__ u64 ld_relaxed(u64 a)
{
	u64 v;
	asm volatile("ld.relaxed.gpu.global.b64 %0, [%1];"
		     : "=l"(v)
		     : "l"(a)
		     : "memory");
	return v;
}
static __device__ __forceinline__ u64 ld_acquire(u64 a)
{
	u64 v;
	asm volatile("ld.acquire.gpu.global.b64 %0, [%1];"
		     : "=l"(v)
		     : "l"(a)
		     : "memory");
	return v;
}
static __device__ __forceinline__ u64 atom_add_acq_rel(u64 a, u64 v)
{
	u64 o;
	asm volatile("atom.acq_rel.gpu.global.add.u64 %0, [%1], %2;"
		     : "=l"(o)
		     : "l"(a), "l"(v)
		     : "memory");
	return o;
}
static __device__ __forceinline__ u64 atom_add_relaxed(u64 a, u64 v)
{
	u64 o;
	asm volatile("atom.relaxed.gpu.global.add.u64 %0, [%1], %2;"
		     : "=l"(o)
		     : "l"(a), "l"(v)
		     : "memory");
	return o;
}
static __device__ __forceinline__ u64 atom_cas(u64 a, u64 cmp, u64 nv)
{
	u64 o;
	asm volatile("atom.relaxed.gpu.global.cas.b64 %0, [%1], %2, %3;"
		     : "=l"(o)
		     : "l"(a), "l"(cmp), "l"(nv)
		     : "memory");
	return o;
}
static __device__ __forceinline__ void red_add(u64 a, u64 v)
{
	asm volatile("red.relaxed.gpu.global.add.u64 [%0], %1;" ::"l"(a),
		     "l"(v)
		     : "memory");
}
static __device__ __forceinline__ void red_max(u64 a, u64 v)
{
	asm volatile("red.relaxed.gpu.global.max.u64 [%0], %1;" ::"l"(a),
		     "l"(v)
		     : "memory");
}
static __device__ __forceinline__ void st_release(u64 a, u64 v)
{
	asm volatile("st.release.gpu.global.b64 [%0], %1;" ::"l"(a), "l"(v)
		     : "memory");
}
#if BPFTIME_DEVHASH_FAULT == 1
static __device__ __forceinline__ void st_plain(u64 a, u64 v)
{
	asm volatile("st.global.b64 [%0], %1;" ::"l"(a), "l"(v) : "memory");
}
#endif
static __device__ __forceinline__ void nsleep(u32 ns)
{
	asm volatile("nanosleep.u32 %0;" ::"r"(ns) : "memory");
}
static __device__ __forceinline__ u32 smid()
{
	u32 r;
	asm volatile("mov.u32 %0, %%smid;" : "=r"(r));
	return r;
}
static __device__ __forceinline__ u32 laneid()
{
	u32 r;
	asm volatile("mov.u32 %0, %%laneid;" : "=r"(r));
	return r;
}
static __device__ __forceinline__ u64 gtimer()
{
	u64 t;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	return t;
}

// ------------------------------------------------------------- the map
struct map_ref {
	u64 base; // generic address of the block (returned pointers)
	u64 g; // its .global address
	layout l;
	u32 shard;
};

static __device__ __forceinline__ map_ref open_map(u64 base, u64 max_entries,
						   u64 shards)
{
	map_ref m;
	m.base = base;
	m.g = to_global(base);
	m.l = layout_for(max_entries, shards);
	m.shard = shard_index(smid(), laneid(), shards);
	return m;
}
static __device__ __forceinline__ u64 ctl(const map_ref &m, u32 word)
{
	return m.g + ctl_offset(word);
}
static __device__ __forceinline__ void count(const map_ref &m, u32 counter)
{
	red_add(m.g + counter_offset(m.l, counter, m.shard), 1);
}

#ifdef BPFTIME_DEVHASH_TEST
static __device__ __forceinline__ void diag(const map_ref &m, u32 d,
					    u64 v = 1)
{
	red_add(ctl(m, CTL_TEST_DIAG + d), v);
}
static __device__ __forceinline__ void outcome(const map_ref &m, u32 code)
{
	const u64 arr = ld_relaxed(ctl(m, CTL_TEST_OUTCOMES));
	if (arr != 0) {
		const u64 idx = (u64)blockIdx.x * blockDim.x + threadIdx.x;
		*(volatile unsigned char *)(arr + idx) = (unsigned char)code;
	}
}
#else
static __device__ __forceinline__ void diag(const map_ref &, u32, u64 = 1)
{
}
static __device__ __forceinline__ void outcome(const map_ref &, u32)
{
}
#endif

static __device__ __forceinline__ u64 eexist(const map_ref &m, u32 oc)
{
	count(m, CNT_EEXIST);
	if (oc == OC_EEXIST_UNSTABLE)
		count(m, CNT_UNSTABLE_EEXIST);
	outcome(m, oc);
	return R_EEXIST;
}
static __device__ __forceinline__ u64 e2big(const map_ref &m)
{
	count(m, CNT_E2BIG);
	outcome(m, OC_E2BIG);
	return R_E2BIG;
}

// Bounded wait for K's slot to leave claiming: 1 live, 2 refused, 0 the
// bound ran out (still claiming).
static __device__ __forceinline__ int wait_resolved(const map_ref &m, u64 a,
						 u32 key)
{
	diag(m, D_WAITS);
	const u64 t0 = gtimer();
	u32 total = 0, d = 64;
	int res = 0;
	for (;;) {
		const u64 h = ld_relaxed(a);
		if (!holds_key(h, key))
			diag(m, D_KEY_CHANGED); // a claimed slot never changes key
		if (state_of(h) == ST_LIVE) {
			res = 1;
			break;
		}
		if (state_of(h) == ST_REFUSED) {
			res = 2;
			break;
		}
		if (total >= BPFTIME_DEVHASH_WAIT_BOUND)
			break;
		nsleep(d);
		total += d;
		d = d < 2048 ? d * 2 : 2048;
	}
#ifdef BPFTIME_DEVHASH_TEST
	red_max(ctl(m, CTL_TEST_DIAG + D_WAIT_MAX_NS), gtimer() - t0);
#else
	(void)t0;
#endif
	return res;
}

static __device__ __forceinline__ u64 after_wait(const map_ref &m, int w)
{
	if (w == 1)
		return eexist(m, OC_EEXIST_WAITED);
	if (w == 2) {
		diag(m, D_E2BIG_SAW_REFUSED);
		return e2big(m);
	}
	return eexist(m, OC_EEXIST_UNSTABLE);
}

// Every slot walked without K or an empty slot: wait (bounded) until the
// count shows the map full, then E2BIG.
static __device__ __forceinline__ u64 full_directory(const map_ref &m)
{
	count(m, CNT_INSERT_FULLWALK);
	u32 total = 0, d = 64;
	for (;;) {
		if (ld_acquire(ctl(m, CTL_COUNT)) >= m.l.max_entries) {
			diag(m, D_E2BIG_FULLDIR);
			return e2big(m);
		}
		if (total >= BPFTIME_DEVHASH_WAIT_BOUND) {
			count(m, CNT_UNSTABLE_E2BIG);
			return e2big(m);
		}
		nsleep(d);
		total += d;
		d = d < 2048 ? d * 2 : 2048;
	}
}

// The winner's add into its shard of row: guarded so that no build (a
// control without the count check) writes past the rows.
static __device__ __forceinline__ bool add_to_row(const map_ref &m, u64 row,
						  u64 value)
{
	if (row >= m.l.max_entries) {
		diag(m, D_ROW_OVERFLOW);
		return false;
	}
	red_add(m.g + value_offset(m.l, row, m.shard), value);
	return true;
}

#if BPFTIME_DEVHASH_FAULT == 6
// Control 6, round one's order: reserve at the empty slot before the CAS,
// give back on a loss, rows from a separate counter.
static __device__ __forceinline__ u64 insert_v2(const map_ref &m, u32 key,
						u64 value)
{
	const u64 mask = m.l.slots - 1;
	u64 i = home_slot(key, m.l.log_slots);
	bool reserved = false;
	const u64 cnt = ctl(m, CTL_COUNT);
	for (u64 n = 0; n < m.l.slots; n++) {
		const u64 a = m.g + slot_offset(m.l, i);
		const u64 h = ld_relaxed(a);
		if (state_of(h) != ST_EMPTY) {
			if (holds_key(h, key)) {
				u64 r = state_of(h) == ST_LIVE ?
						eexist(m, OC_EEXIST_LIVE) :
						after_wait(m,
							   wait_resolved(m, a,
									 key));
				if (reserved) {
					atom_add_relaxed(cnt, ~0ull);
					diag(m, D_GIVEBACK);
				}
				return r;
			}
			i = (i + 1) & mask;
			continue;
		}
		if (!reserved) {
			if (ld_relaxed(cnt) >= m.l.max_entries) {
				diag(m, D_E2BIG_PRECHECK);
				return e2big(m);
			}
			if (atom_add_relaxed(cnt, 1) >= m.l.max_entries) {
				atom_add_relaxed(cnt, ~0ull);
				diag(m, D_E2BIG_AFTER_ADD);
				return e2big(m);
			}
			reserved = true;
		}
		const u64 old = atom_cas(a, 0, make_header(key, 0, ST_CLAIMING));
		if (old == 0) {
			const u64 row = atom_add_relaxed(
				ctl(m, CTL_TEST_DIAG + D_NEXT_ROW), 1);
			const bool ok = add_to_row(m, row, value);
			st_release(a, make_header(key, row, ST_LIVE));
			outcome(m, ok ? OC_WON : OC_ANOM_ROW);
			return 0;
		}
		if (holds_key(old, key)) {
			diag(m, D_LOST_SAME);
			if (reserved) {
				atom_add_relaxed(cnt, ~0ull);
				diag(m, D_GIVEBACK);
			}
			return after_wait(m, wait_resolved(m, a, key));
		}
		diag(m, D_LOST_OTHER);
		i = (i + 1) & mask;
	}
	if (reserved) {
		atom_add_relaxed(cnt, ~0ull);
		diag(m, D_GIVEBACK);
	}
	return full_directory(m);
}
#else
// insert(K, V, BPF_NOEXIST), protocol v2
static __device__ __forceinline__ u64 insert_v2(const map_ref &m, u32 key,
						u64 value)
{
	const u64 mask = m.l.slots - 1;
	u64 i = home_slot(key, m.l.log_slots);
	const u64 cnt = ctl(m, CTL_COUNT);
	for (u64 n = 0; n < m.l.slots;) {
		const u64 a = m.g + slot_offset(m.l, i);
		const u64 h = ld_relaxed(a);
		if (state_of(h) != ST_EMPTY) {
			if (holds_key(h, key)) {
				if (state_of(h) == ST_LIVE)
					return eexist(m, OC_EEXIST_LIVE);
				if (state_of(h) == ST_REFUSED) {
					diag(m, D_E2BIG_SAW_REFUSED);
					return e2big(m);
				}
				diag(m, D_WALK_SAW_CLAIMING);
				return after_wait(m, wait_resolved(m, a, key));
			}
			// another key's slot, or opaque: walk on
			i = (i + 1) & mask;
			n++;
			continue;
		}
		// an empty slot: the reservation count first (acquire)
		if (ld_acquire(cnt) >= m.l.max_entries) {
#if BPFTIME_DEVHASH_FAULT == 5
			diag(m, D_E2BIG_PRECHECK);
			return e2big(m);
#else
			if (state_of(ld_relaxed(a)) == ST_EMPTY) {
				diag(m, D_E2BIG_PRECHECK);
				return e2big(m);
			}
			diag(m, D_PRECHECK_SLOT_TAKEN);
			continue; // taken meanwhile: handle the slot again
#endif
		}
		const u64 claim = make_header(key, 0, ST_CLAIMING);
#if BPFTIME_DEVHASH_FAULT == 1
		st_plain(a, claim);
		const u64 old = 0; // the plain store "always wins"
#else
		const u64 old = atom_cas(a, 0, claim);
#endif
		if (old == 0) {
			// won: reserve (release: the claim before it; acquire:
			// the other winners' adds)
			const u64 r0 = atom_add_acq_rel(cnt, 1);
#if BPFTIME_DEVHASH_FAULT != 2
			if (r0 >= m.l.max_entries) {
				st_release(a, make_header(key, 0, ST_REFUSED));
				diag(m, D_REFUSED_PUBLISHED);
				diag(m, D_E2BIG_AFTER_ADD);
				return e2big(m);
			}
#endif
			const bool ok = add_to_row(m, r0, value);
			st_release(a, make_header(key, r0, ST_LIVE));
			outcome(m, ok ? OC_WON : OC_ANOM_ROW);
			return 0;
		}
		if (holds_key(old, key)) {
			diag(m, D_LOST_SAME);
			return after_wait(m, wait_resolved(m, a, key));
		}
		diag(m, D_LOST_OTHER);
		i = (i + 1) & mask;
		n++;
	}
	return full_directory(m);
}
#endif
} // namespace devhash_impl

using namespace devhash_impl;

extern "C" __device__ __noinline__ u64 __bpftime_devhash_lookup(
	u64 base, u64 key, u64 max_entries, u64 shards)
{
	if (base == 0)
		return 0;
	const map_ref m = open_map(base, max_entries, shards);
	const u32 k = (u32)key;
	const u64 mask = m.l.slots - 1;
	u64 i = home_slot(k, m.l.log_slots);
	for (u64 n = 0; n < m.l.slots; n++) {
		const u64 h = ld_relaxed(m.g + slot_offset(m.l, i));
		if (state_of(h) == ST_EMPTY)
			return 0;
		if (holds_key(h, k)) {
			if (state_of(h) != ST_LIVE)
				return 0; // claim in flight, or refused
			const u64 row = row_of(h);
			if (row >= max_entries)
				return 0;
			return base + value_offset(m.l, row, m.shard);
		}
		i = (i + 1) & mask;
	}
	count(m, CNT_LOOKUP_FULLWALK);
	return 0;
}

extern "C" __device__ __noinline__ u64
__bpftime_devhash_insert(u64 base, u64 key, u64 value_ptr, u64 flags,
			 u64 max_entries, u64 shards)
{
	if (base == 0)
		return R_NO_COPY;
	const map_ref m = open_map(base, max_entries, shards);
	if ((flags & 0xffffffffull) != NOEXIST) {
		count(m, CNT_BAD_FLAGS);
		outcome(m, OC_EINVAL);
		return R_EINVAL;
	}
	const u64 value = *(const u64 *)value_ptr;
	return insert_v2(m, (u32)key, value);
}

extern "C" __device__ __noinline__ u64 __bpftime_devhash_layout_fp()
{
	return DEVHASH_LAYOUT_FP;
}

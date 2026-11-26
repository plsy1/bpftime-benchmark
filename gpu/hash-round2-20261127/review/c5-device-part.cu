// C5: the device hash map's key directory, protocol v2 (claim, then reserve), by hand.
// Standalone CUDA test, no bpftime. Rules: ../../RULES-round2.md section C5.
// From C4 (../../c4/src/c4.cu, round one). Design: results gpu/hash-design-20261126.
//
// v2: a thread that finds an empty slot first CASes it to claiming(K); only the
// winner then adds 1 to the reservation count (atom.acq_rel). Old value below
// max_entries: row = old value, add, publish live(K, row) with st.release.
// Otherwise the slot is published refused(K) (permanent) and the winner gets
// E2BIG. Before its CAS a thread reads the count with ld.acquire; at or above
// max_entries it reads the slot again and returns E2BIG only if it is still
// empty. The count never goes down.
//
// Builds selected by -DFAULT=:
//   0  protocol v2
//   1  control: claim with a plain st.global.b64 instead of the CAS
//   2  control: winners never check the count (run at max_entries 48)
//   5  control: the precheck returns E2BIG without reading the slot again
//   6  control: round one's order (reserve at the empty slot before the CAS,
//      give back on a loss; rows from a separate counter)
//
// Every access to the directory, the control words, the rows and the counters
// goes through the inline-PTX helpers below on .global addresses
// (cvta.to.global of the cudaMalloc pointer). The only compiler-emitted global
// store is the per-thread outcome byte (a thread-private cell).

#include <cuda_runtime.h>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <array>
#include <vector>

#ifndef FAULT
#define FAULT 0
#endif

typedef unsigned long long u64;
typedef unsigned int u32;

#define NSHARDS 256
#ifndef WAIT_BOUND_NS
#define WAIT_BOUND_NS 100000u // requested nanosleep total before giving up
#endif

// header states (bits 2..0)
#define ST_EMPTY 0ull
#define ST_CLAIMING 1ull
#define ST_LIVE 2ull
#define ST_REFUSED 3ull // a winner whose reservation failed; permanent

// outcome codes (per thread per launch)
enum {
	OC_NONE = 0,
	OC_HIT = 1,
	OC_WON = 2,
	OC_EEXIST_LIVE = 3, // walk met live K
	OC_EEXIST_WAITED = 4, // met claiming K (or lost the CAS to K), waited, saw live
	OC_EEXIST_UNSTABLE = 5, // waited up to the bound, still claiming
	OC_E2BIG = 6, // any E2BIG; the path is counted in the control words
	OC_ANOM_WALK = 7, // walk passed S slots
	OC_ANOM_ROW = 8, // won, but row >= rows_cap (add skipped)
	OC_FLAG_THEN_NULL = 0x10, // EEXIST, then the thread's own lookup returned NULL
};

// control words (u64 each)
enum {
	C_BOUND = 0,
	C_NEXT_ROW, // FAULT 6 only: rows from a separate counter
	C_WALK_OVERFLOW,
	C_ROW_OVERFLOW,
	C_LOST_SAME, // CAS lost to the same key
	C_LOST_OTHER, // CAS lost to another key
	C_WAITS, // bounded waits entered
	C_WAIT_SLEEPS, // nanosleep calls in waits
	C_WAIT_MAX_NS, // longest wait, globaltimer
	C_EEXIST_THEN_NULL,
	C_UNSTABLE_THEN_NULL,
	C_BLOCKS_DONE,
	C_FIRST_START_NS, // min globaltimer at block start (host sets ~0)
	C_E2BIG_PRECHECK, // precheck: count >= max and the slot still empty
	C_E2BIG_AFTER_ADD, // a winner's add returned >= max: slot refused
	C_GIVEBACK, // FAULT 6 only: reservations given back
	C_WALK_SAW_CLAIMING, // insert walk met claiming K
	C_KEY_CHANGED, // a waited-on slot showed another key (must never happen)
	C_LAST_END_NS, // max globaltimer at block end
	C_WAIT_MAX_REQ_NS, // longest wait, requested nanosleep total
	C_E2BIG_SAW_REFUSED, // walk or wait met refused(K)
	C_PRECHECK_SLOT_TAKEN, // precheck: count >= max but the slot was no longer empty
	C_E2BIG_FULLDIR, // walked all S slots, waited until count >= max
	C_FULLDIR_UNSTABLE, // walked all S slots, the wait ran out (still E2BIG)
	C_REFUSED_PUBLISHED, // winners that published refused
	C_LOOKUP_FULLWALK, // a lookup walked all S slots (absent key, full directory)
	C_NCTL
};

// reader stats (u64 each)
enum {
	R_POLLS = 0,
	R_SEEN_CLAIMING, // polls that saw a claiming header
	R_TRANSITIONS, // slot went empty/claiming -> live while this reader watched it
	R_LIVE_CHECKS, // live headers whose row was summed
	R_ZERO_SUM, // live header, row sum 0 (violation)
	R_DECREASE, // row sum went down between checks (violation)
	R_SUM_CHANGES, // row sum changed between checks (overlap evidence)
	R_START_NS, // min globaltimer at reader start (host sets ~0)
	R_END_NS,
	R_TIMED_OUT,
	R_FIRST_TRANSITION_SUM_MIN, // smallest row sum seen at an observed transition (host sets ~0)
	R_NSTAT
};

struct Params {
	u64 dir, rows, ctl, eexist, e2big, unstable, outcomes, rstat;
	u64 seed;
	u32 S, logS, max_entries, rows_cap, n_threads, launch, nkeys;
};

// ---------------------------------------------------------------- PTX helpers
__device__ __forceinline__ u64 to_global(u64 p)
{
	u64 g;
	asm("cvta.to.global.u64 %0, %1;" : "=l"(g) : "l"(p));
	return g;
}
__device__ __forceinline__ u64 ld_relaxed(u64 a)
{
	u64 v;
	asm volatile("ld.relaxed.gpu.global.b64 %0, [%1];" : "=l"(v) : "l"(a) : "memory");
	return v;
}
__device__ __forceinline__ u64 ld_acquire(u64 a)
{
	u64 v;
	asm volatile("ld.acquire.gpu.global.b64 %0, [%1];" : "=l"(v) : "l"(a) : "memory");
	return v;
}
__device__ __forceinline__ u64 atom_add_acq_rel(u64 a, u64 v)
{
	u64 o;
	asm volatile("atom.acq_rel.gpu.global.add.u64 %0, [%1], %2;" : "=l"(o) : "l"(a), "l"(v) : "memory");
	return o;
}
__device__ __forceinline__ void fence_acq_rel()
{
	asm volatile("fence.acq_rel.gpu;" ::: "memory");
}
__device__ __forceinline__ u64 atom_add(u64 a, u64 v)
{
	u64 o;
	asm volatile("atom.relaxed.gpu.global.add.u64 %0, [%1], %2;" : "=l"(o) : "l"(a), "l"(v) : "memory");
	return o;
}
__device__ __forceinline__ u64 atom_cas(u64 a, u64 cmp, u64 nv)
{
	u64 o;
	asm volatile("atom.relaxed.gpu.global.cas.b64 %0, [%1], %2, %3;"
		     : "=l"(o)
		     : "l"(a), "l"(cmp), "l"(nv)
		     : "memory");
	return o;
}
__device__ __forceinline__ void atom_max(u64 a, u64 v)
{
	asm volatile("red.relaxed.gpu.global.max.u64 [%0], %1;" ::"l"(a), "l"(v) : "memory");
}
__device__ __forceinline__ void atom_min(u64 a, u64 v)
{
	asm volatile("red.relaxed.gpu.global.min.u64 [%0], %1;" ::"l"(a), "l"(v) : "memory");
}
__device__ __forceinline__ void red_add(u64 a, u64 v)
{
	asm volatile("red.global.add.u64 [%0], %1;" ::"l"(a), "l"(v) : "memory");
}
__device__ __forceinline__ void st_release(u64 a, u64 v)
{
	asm volatile("st.release.gpu.global.b64 [%0], %1;" ::"l"(a), "l"(v) : "memory");
}
__device__ __forceinline__ void st_relaxed(u64 a, u64 v)
{
	asm volatile("st.relaxed.gpu.global.b64 [%0], %1;" ::"l"(a), "l"(v) : "memory");
}
__device__ __forceinline__ void st_plain(u64 a, u64 v)
{
	asm volatile("st.global.b64 [%0], %1;" ::"l"(a), "l"(v) : "memory");
}
__device__ __forceinline__ void nsleep(u32 ns)
{
	asm volatile("nanosleep.u32 %0;" ::"r"(ns) : "memory");
}
__device__ __forceinline__ u32 smid()
{
	u32 r;
	asm volatile("mov.u32 %0, %%smid;" : "=r"(r));
	return r;
}
__device__ __forceinline__ u32 laneid()
{
	u32 r;
	asm volatile("mov.u32 %0, %%laneid;" : "=r"(r));
	return r;
}
__device__ __forceinline__ u64 gtimer()
{
	u64 t;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	return t;
}

// ---------------------------------------------------------------- protocol
struct Ctx {
	u64 dir, rows, ctl, eexist, e2big, unstable, outcomes, rstat;
	u64 seed;
	u32 S, logS, max_entries, rows_cap;
};

__host__ __device__ __forceinline__ u32 home_slot(u32 K, u64 seed, u32 logS)
{
	return (u32)((((u64)K ^ seed) * 0x9E3779B97F4A7C15ull) >> (64 - logS));
}
__host__ __device__ __forceinline__ u32 key_of(u32 j)
{
	return 1000u + 7919u * j;
}
// key index of a thread: one key per thread when nkeys == n_threads, else a
// multiply-shift range reduction (for 64 keys exactly C4's scrambling)
__host__ __device__ __forceinline__ u32 keyidx_of_tid(u32 tid, u32 nkeys, u32 n_threads)
{
	if (nkeys == n_threads)
		return tid;
	return (u32)(((u64)(tid * 2654435761u) * nkeys) >> 32);
}

__device__ __forceinline__ u64 ctlw(const Ctx &c, int i)
{
	return c.ctl + 8ull * (u64)i;
}

// lookup(K): walk from home until K's header or an empty slot. Never waits.
__device__ __forceinline__ u64 lookup(const Ctx &c, u32 K, u32 shard)
{
	u32 i = home_slot(K, c.seed, c.logS);
	for (u32 n = 0; n < c.S; n++) {
		u64 h = ld_relaxed(c.dir + 8ull * i);
		u64 st = h & 7ull;
		if (st == ST_EMPTY)
			return 0;
		if ((u32)(h >> 32) == K) {
			if (st != ST_LIVE)
				return 0; // claim in flight: NULL
			u64 row = (h >> 3) & 0x1FFFFFFFull;
			if (row >= c.rows_cap)
				return 0;
			return c.rows + 8ull * (row * NSHARDS + shard);
		}
		i = (i + 1) & (c.S - 1);
	}
	red_add(ctlw(c, C_LOOKUP_FULLWALK), 1);
	return 0;
}

// bounded wait for K's slot to leave claiming. Returns 1 live, 2 refused, 0 the
// bound ran out (still claiming).
__device__ __noinline__ int wait_resolved(const Ctx &c, u64 a, u32 K)
{
	red_add(ctlw(c, C_WAITS), 1);
	u64 t0 = gtimer();
	u32 total = 0, d = 64, sleeps = 0;
	int res = 0;
	for (;;) {
		u64 h = ld_relaxed(a);
		if ((u32)(h >> 32) != K)
			red_add(ctlw(c, C_KEY_CHANGED), 1);
		u64 st = h & 7ull;
		if (st == ST_LIVE) {
			res = 1;
			break;
		}
		if (st == ST_REFUSED) {
			res = 2;
			break;
		}
		if (total >= WAIT_BOUND_NS)
			break;
		nsleep(d);
		total += d;
		sleeps++;
		d = d < 2048 ? d * 2 : 2048;
	}
	u64 dt = gtimer() - t0;
	if (sleeps)
		red_add(ctlw(c, C_WAIT_SLEEPS), sleeps);
	atom_max(ctlw(c, C_WAIT_MAX_NS), dt);
	atom_max(ctlw(c, C_WAIT_MAX_REQ_NS), total);
	return res;
}

// the outcome of meeting K's slot in claiming state (walk or lost CAS)
__device__ __forceinline__ int after_wait(const Ctx &c, int w)
{
	if (w == 1)
		return OC_EEXIST_WAITED;
	if (w == 2) {
		red_add(ctlw(c, C_E2BIG_SAW_REFUSED), 1);
		return OC_E2BIG;
	}
	return OC_EEXIST_UNSTABLE;
}

__device__ __forceinline__ void publish(u64 a, u64 h)
{
	st_release(a, h);
}

// the whole directory walked without K or an empty slot: wait (bounded) until
// the count shows the map full, then E2BIG
__device__ __noinline__ int full_directory(const Ctx &c)
{
	red_add(ctlw(c, C_WALK_OVERFLOW), 1);
	u32 total = 0, d = 64;
	for (;;) {
		if (ld_acquire(ctlw(c, C_BOUND)) >= c.max_entries) {
			red_add(ctlw(c, C_E2BIG_FULLDIR), 1);
			return OC_E2BIG;
		}
		if (total >= WAIT_BOUND_NS) {
			red_add(ctlw(c, C_FULLDIR_UNSTABLE), 1);
			return OC_E2BIG;
		}
		nsleep(d);
		total += d;
		d = d < 2048 ? d * 2 : 2048;
	}
}

#if FAULT == 6
// round one's order, for control 6: reserve at the empty slot before the CAS,
// give back on a loss, rows from a separate counter
__device__ __noinline__ int insert(const Ctx &c, u32 K, u32 shard)
{
	u32 i = home_slot(K, c.seed, c.logS);
	bool reserved = false;
	for (u32 n = 0; n < c.S; n++) {
		u64 a = c.dir + 8ull * i;
		u64 h = ld_relaxed(a);
		u64 st = h & 7ull;
		if (st != ST_EMPTY) {
			if ((u32)(h >> 32) == K) {
				int r = st == ST_LIVE ? OC_EEXIST_LIVE : after_wait(c, wait_resolved(c, a, K));
				if (reserved) {
					atom_add(ctlw(c, C_BOUND), ~0ull);
					red_add(ctlw(c, C_GIVEBACK), 1);
				}
				return r;
			}
			i = (i + 1) & (c.S - 1);
			continue;
		}
		if (!reserved) {
			if (ld_relaxed(ctlw(c, C_BOUND)) >= c.max_entries) {
				red_add(ctlw(c, C_E2BIG_PRECHECK), 1);
				return OC_E2BIG;
			}
			u64 old = atom_add(ctlw(c, C_BOUND), 1);
			if (old >= c.max_entries) {
				atom_add(ctlw(c, C_BOUND), ~0ull);
				red_add(ctlw(c, C_E2BIG_AFTER_ADD), 1);
				return OC_E2BIG;
			}
			reserved = true;
		}
		u64 old = atom_cas(a, 0ull, ((u64)K << 32) | ST_CLAIMING);
		if (old == 0ull) {
			u64 row = atom_add(ctlw(c, C_NEXT_ROW), 1);
			int r = OC_WON;
			if (row < c.rows_cap) {
				red_add(c.rows + 8ull * (row * NSHARDS + shard), 1);
			} else {
				red_add(ctlw(c, C_ROW_OVERFLOW), 1);
				r = OC_ANOM_ROW;
			}
			publish(a, ((u64)K << 32) | ((row & 0x1FFFFFFFull) << 3) | ST_LIVE);
			return r;
		}
		if ((u32)(old >> 32) == K) {
			red_add(ctlw(c, C_LOST_SAME), 1);
			if (reserved) {
				atom_add(ctlw(c, C_BOUND), ~0ull);
				red_add(ctlw(c, C_GIVEBACK), 1);
			}
			return after_wait(c, wait_resolved(c, a, K));
		}
		red_add(ctlw(c, C_LOST_OTHER), 1);
		i = (i + 1) & (c.S - 1);
	}
	if (reserved) {
		atom_add(ctlw(c, C_BOUND), ~0ull);
		red_add(ctlw(c, C_GIVEBACK), 1);
	}
	return full_directory(c);
}
#else
// insert(K, 1, BPF_NOEXIST), protocol v2
__device__ __noinline__ int insert(const Ctx &c, u32 K, u32 shard)
{
	u32 i = home_slot(K, c.seed, c.logS);
	for (u32 n = 0; n < c.S;) {
		u64 a = c.dir + 8ull * i;
		u64 h = ld_relaxed(a);
		u64 st = h & 7ull;
		if (st != ST_EMPTY) {
			if ((u32)(h >> 32) == K) {
				if (st == ST_LIVE)
					return OC_EEXIST_LIVE;
				if (st == ST_REFUSED) {
					red_add(ctlw(c, C_E2BIG_SAW_REFUSED), 1);
					return OC_E2BIG;
				}
				red_add(ctlw(c, C_WALK_SAW_CLAIMING), 1);
				return after_wait(c, wait_resolved(c, a, K));
			}
			i = (i + 1) & (c.S - 1);
			n++;
			continue;
		}
		// empty slot: precheck the count (acquire)
		if (ld_acquire(ctlw(c, C_BOUND)) >= c.max_entries) {
#if FAULT == 5
			red_add(ctlw(c, C_E2BIG_PRECHECK), 1);
			return OC_E2BIG;
#else
			if ((ld_relaxed(a) & 7ull) == ST_EMPTY) {
				red_add(ctlw(c, C_E2BIG_PRECHECK), 1);
				return OC_E2BIG;
			}
			red_add(ctlw(c, C_PRECHECK_SLOT_TAKEN), 1);
			continue; // the slot was taken meanwhile: handle it again
#endif
		}
		u64 claim = ((u64)K << 32) | ST_CLAIMING;
#if FAULT == 1
		st_plain(a, claim);
		u64 old = 0; // the plain store "always wins"
#else
		u64 old = atom_cas(a, 0ull, claim);
#endif
		if (old == 0ull) {
			// won: reserve (release: the CAS before it; acquire: pairs with
			// the other winners' adds)
			u64 r0 = atom_add_acq_rel(ctlw(c, C_BOUND), 1);
#if FAULT != 2
			if (r0 >= c.max_entries) {
				publish(a, ((u64)K << 32) | ST_REFUSED);
				red_add(ctlw(c, C_REFUSED_PUBLISHED), 1);
				red_add(ctlw(c, C_E2BIG_AFTER_ADD), 1);
				return OC_E2BIG;
			}
#endif
			u64 row = r0;
			int r = OC_WON;
			if (row < c.rows_cap) {
				red_add(c.rows + 8ull * (row * NSHARDS + shard), 1);
			} else {
				red_add(ctlw(c, C_ROW_OVERFLOW), 1);
				r = OC_ANOM_ROW;
			}
			publish(a, ((u64)K << 32) | ((row & 0x1FFFFFFFull) << 3) | ST_LIVE);
			return r;
		}
		if ((u32)(old >> 32) == K) {
			red_add(ctlw(c, C_LOST_SAME), 1);
			return after_wait(c, wait_resolved(c, a, K));
		}
		red_add(ctlw(c, C_LOST_OTHER), 1);
		i = (i + 1) & (c.S - 1);
		n++;
	}
	return full_directory(c);
}
#endif

__device__ __forceinline__ Ctx make_ctx(const Params &p)
{
	Ctx c;
	c.dir = to_global(p.dir);
	c.rows = to_global(p.rows);
	c.ctl = to_global(p.ctl);
	c.eexist = to_global(p.eexist);
	c.e2big = to_global(p.e2big);
	c.unstable = to_global(p.unstable);
	c.outcomes = p.outcomes;
	c.rstat = to_global(p.rstat);
	c.seed = p.seed;
	c.S = p.S;
	c.logS = p.logS;
	c.max_entries = p.max_entries;
	c.rows_cap = p.rows_cap;
	return c;
}

__global__ void __launch_bounds__(256) k_ops(Params p)
{
	Ctx c = make_ctx(p);
	u32 tid = blockIdx.x * blockDim.x + threadIdx.x;
	if (threadIdx.x == 0)
		atom_min(ctlw(c, C_FIRST_START_NS), gtimer());
	if (tid < p.n_threads) {
		u32 j = keyidx_of_tid(tid, p.nkeys, p.n_threads);
		u32 K = key_of(j);
		u32 shard = (smid() & 7u) * 32u + laneid();
		unsigned char oc;
		u64 v = lookup(c, K, shard);
		if (v) {
			red_add(v, 1);
			oc = OC_HIT;
		} else {
			int r = insert(c, K, shard);
			oc = (unsigned char)r;
			if (r == OC_EEXIST_LIVE || r == OC_EEXIST_WAITED || r == OC_EEXIST_UNSTABLE) {
				red_add(c.eexist + 8ull * j, 1);
				if (r == OC_EEXIST_UNSTABLE)
					red_add(c.unstable + 8ull * j, 1);
				// "EEXIST, then not NULL": the thread's own lookup after EEXIST
				if (lookup(c, K, shard) == 0) {
					oc |= OC_FLAG_THEN_NULL;
					red_add(ctlw(c, r == OC_EEXIST_UNSTABLE ? C_UNSTABLE_THEN_NULL
										: C_EEXIST_THEN_NULL),
						1);
				}
			} else if (r == OC_E2BIG) {
				red_add(c.e2big + 8ull * j, 1);
			}
		}
		((unsigned char *)p.outcomes)[(u64)p.launch * p.n_threads + tid] = oc;
	}
	__syncthreads();
	if (threadIdx.x == 0) {
		atom_max(ctlw(c, C_LAST_END_NS), gtimer());
		red_add(ctlw(c, C_BLOCKS_DONE), 1);
	}
}

// Concurrent reader: thread t watches slot t. Acquire pattern on the header

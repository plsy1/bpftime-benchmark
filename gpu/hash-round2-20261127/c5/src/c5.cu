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
// ("a strong read on M followed by an acquire memory fence"), then sums the
// live row's 256 shards with ld.relaxed. A live header's row sum must be >= 1.
__global__ void k_reader(Params p, u64 total_blocks, u64 tmax_ns)
{
	Ctx c = make_ctx(p);
	u32 t = blockIdx.x * blockDim.x + threadIdx.x;
	u64 t0 = gtimer();
	atom_min(c.rstat + 8ull * R_START_NS, t0);
	if (t >= c.S)
		return;
	u64 a = c.dir + 8ull * t;
	u64 polls = 0, seen_claiming = 0, transitions = 0, live_checks = 0, zero_sum = 0,
	    decrease = 0, sum_changes = 0, timed_out = 0, tr_min = ~0ull;
	u64 last_state = ST_EMPTY, last_sum = 0;
	bool live_seen = false;
	for (;;) {
		if (ld_relaxed(ctlw(c, C_BLOCKS_DONE)) >= total_blocks)
			break;
		if (gtimer() - t0 > tmax_ns) {
			timed_out = 1;
			break;
		}
		u64 h = ld_relaxed(a);
		fence_acq_rel();
		u64 st = h & 7ull;
		polls++;
		if (st == ST_CLAIMING)
			seen_claiming++;
		if (st == ST_LIVE) {
			u64 row = (h >> 3) & 0x1FFFFFFFull;
			if (row < c.rows_cap) {
				u64 base = c.rows + 8ull * row * NSHARDS;
				u64 sum = 0;
#pragma unroll 64
				for (u32 s = 0; s < NSHARDS; s++)
					sum += ld_relaxed(base + 8ull * s);
				live_checks++;
				if (sum == 0)
					zero_sum++;
				if (!live_seen && last_state != ST_LIVE && polls > 1) {
					transitions++;
					if (sum < tr_min)
						tr_min = sum;
				}
				if (live_seen) {
					if (sum < last_sum)
						decrease++;
					if (sum != last_sum)
						sum_changes++;
				}
				last_sum = sum;
				live_seen = true;
			}
		}
		last_state = st;
	}
	red_add(c.rstat + 8ull * R_POLLS, polls);
	red_add(c.rstat + 8ull * R_SEEN_CLAIMING, seen_claiming);
	red_add(c.rstat + 8ull * R_TRANSITIONS, transitions);
	red_add(c.rstat + 8ull * R_LIVE_CHECKS, live_checks);
	red_add(c.rstat + 8ull * R_ZERO_SUM, zero_sum);
	red_add(c.rstat + 8ull * R_DECREASE, decrease);
	red_add(c.rstat + 8ull * R_SUM_CHANGES, sum_changes);
	red_add(c.rstat + 8ull * R_TIMED_OUT, timed_out);
	atom_max(c.rstat + 8ull * R_END_NS, gtimer());
	atom_min(c.rstat + 8ull * R_FIRST_TRANSITION_SUM_MIN, tr_min);
}

// ---------------------------------------------------------------- host
#define CK(x)                                                                                      \
	do {                                                                                       \
		cudaError_t e_ = (x);                                                              \
		if (e_ != cudaSuccess) {                                                           \
			fprintf(stderr, "CUDA error %s at %s:%d: %s\n", cudaGetErrorName(e_), __FILE__, \
				__LINE__, cudaGetErrorString(e_));                                \
			exit(2);                                                                   \
		}                                                                                  \
	} while (0)

static u64 splitmix64(u64 x)
{
	x += 0x9E3779B97F4A7C15ull;
	x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
	x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
	return x ^ (x >> 31);
}


static const char *fault_name()
{
	switch (FAULT) {
	case 0:
		return "protocol-v2";
	case 1:
		return "ctl1-plain-store-claim";
	case 2:
		return "ctl2-no-count-check";
	case 5:
		return "ctl5-precheck-without-reread";
	case 6:
		return "ctl6-round-one-order";
	}
	return "?";
}

struct Agg {
	u64 trials = 0, trials_with_violation = 0;
	u64 viol[32] = { 0 };
	u64 e2big = 0, eexist = 0, unstable = 0, unstable_then_null = 0, eexist_then_null = 0;
	u64 waits = 0, wait_sleeps = 0, wait_max_ns = 0, wait_max_req = 0, lost_same = 0, lost_other = 0,
	    walk_saw_claiming = 0, giveback = 0, e2big_pre = 0, e2big_after = 0, e2big_saw_refused = 0,
	    precheck_taken = 0, e2big_fulldir = 0, fulldir_unstable = 0, insert_fullwalk = 0,
	    lookup_fullwalk = 0;
	u64 refused_sum = 0, refused_max = 0, live_min = ~0ull, live_max = 0;
	u64 keys_off_home = 0, max_dist = 0;
	u64 r_polls = 0, r_claiming = 0, r_transitions = 0, r_live = 0, r_zero = 0, r_decr = 0,
	    r_changes = 0, r_timeout = 0, r_overlap_trials = 0;
	u64 r_min_tr_sum = ~0ull;
	u64 e2big_tmin = ~0ull, e2big_tmax = 0, eexist_tmin = ~0ull, eexist_tmax = 0;
};

enum {
	V_TWO_SLOTS = 0,
	V_LIVE_GT_MAX,
	V_LIVE_COUNT,
	V_LEFT_CLAIMING,
	V_KEY_SUM,
	V_TOTAL_SUM,
	V_GARBAGE,
	V_ROW_DUP,
	V_ROW_RANGE,
	V_BOOKKEEPING,
	V_WINNERS,
	V_TALLY,
	V_EEXIST_THEN_NULL,
	V_CHAIN,
	V_ORPHAN,
	V_ANOM,
	V_READER_ZERO,
	V_READER_DECREASE,
	V_KEY_CHANGED,
	V_E2BIG_WITH_ROOM,
	V_E2BIG_LIVE_KEY,
	V_EEXIST_NOT_LIVE,
	V_N
};
static const char *vname[V_N] = { "key_in_two_slots",   "live_gt_max",        "live_ne_expected",
				  "left_claiming",      "key_sum_mismatch",   "total_sum_mismatch",
				  "garbage_header",     "row_in_two_headers", "row_out_of_range",
				  "bookkeeping",        "winners_ne_1",       "tally_ne_counter",
				  "eexist_then_null",   "probe_chain_gap",    "orphan_adds",
				  "anomaly",            "reader_zero_sum",    "reader_decrease",
				  "key_changed",        "e2big_with_room",    "e2big_on_live_key",
				  "eexist_on_key_not_live" };

// 7919^-1 mod 2^32 (Newton), so a key value maps back to its index
static u32 inv7919()
{
	u32 x = 7919u;
	for (int i = 0; i < 5; i++)
		x *= 2u - 7919u * x;
	return x;
}

int main(int argc, char **argv)
{
	u32 max_entries = 128, trials = 1, launches = 10, n_threads = 1000000, prefill = 0, keys = 64;
	int reader = 0, detail = 1;
	u64 seed0 = 0x0123456789ABCDEFull;
	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		auto nx = [&]() { return (i + 1 < argc) ? argv[++i] : (char *)"0"; };
		if (a == "--max")
			max_entries = strtoul(nx(), 0, 0);
		else if (a == "--trials")
			trials = strtoul(nx(), 0, 0);
		else if (a == "--launches")
			launches = strtoul(nx(), 0, 0);
		else if (a == "--threads")
			n_threads = strtoul(nx(), 0, 0);
		else if (a == "--seed")
			seed0 = strtoull(nx(), 0, 0);
		else if (a == "--reader")
			reader = atoi(nx());
		else if (a == "--prefill")
			prefill = strtoul(nx(), 0, 0);
		else if (a == "--detail")
			detail = atoi(nx());
		else if (a == "--keys") // 0: one key per thread
			keys = strtoul(nx(), 0, 0);
		else {
			fprintf(stderr, "unknown arg %s\n", a.c_str());
			return 2;
		}
	}
	const u32 nkeys = keys ? keys : n_threads;
	if (nkeys > n_threads) {
		fprintf(stderr, "more keys than threads\n");
		return 2;
	}
	u32 S = 1, logS = 0;
	while (S < 2 * max_entries) {
		S <<= 1;
		logS++;
	}
	u32 rows_cap = S; // slack: controls may take more rows than max_entries
	const u32 block = 256;
	u32 grid = (n_threads + block - 1) / block;
	if (prefill > nkeys || prefill > max_entries) {
		fprintf(stderr, "prefill too large\n");
		return 2;
	}
	const u32 INV = inv7919();

	cudaDeviceProp prop;
	CK(cudaGetDeviceProperties(&prop, 0));
	printf("C5 build=%s FAULT=%d device=\"%s\" sm_%d%d SMs=%d\n", fault_name(), FAULT, prop.name,
	       prop.major, prop.minor, prop.multiProcessorCount);
	printf("config max_entries=%u S=%u logS=%u rows_cap=%u keys=%u%s threads=%u grid=%ux%u launches=%u "
	       "trials=%u reader=%d prefill=%u seed0=0x%016llx wait_bound_ns=%u\n",
	       max_entries, S, logS, rows_cap, nkeys, keys ? "" : " (one per thread)", n_threads, grid, block,
	       launches, trials, reader, prefill, seed0, WAIT_BOUND_NS);

	std::vector<u64> per_launch(nkeys, 0), attempts(nkeys, 0);
	for (u32 t = 0; t < n_threads; t++)
		per_launch[keyidx_of_tid(t, nkeys, n_threads)]++;
	u64 keys_used = 0;
	for (u32 j = 0; j < nkeys; j++) {
		attempts[j] = per_launch[j] * launches;
		if (per_launch[j])
			keys_used++;
	}
	{
		u64 mn = ~0ull, mx = 0;
		for (u32 j = 0; j < nkeys; j++) {
			mn = per_launch[j] < mn ? per_launch[j] : mn;
			mx = per_launch[j] > mx ? per_launch[j] : mx;
		}
		printf("attempts per key per launch: min=%llu max=%llu; keys with attempts=%llu (K = 1000 + 7919*j mod 2^32)\n",
		       mn, mx, keys_used);
	}
	const u64 expect_live = keys_used < max_entries ? keys_used : max_entries;

	u64 *d_dir, *d_rows, *d_ctl, *d_eexist, *d_e2big, *d_unstable, *d_rstat;
	unsigned char *d_out;
	size_t rows_words = (size_t)rows_cap * NSHARDS;
	CK(cudaMalloc(&d_dir, 8ull * S));
	CK(cudaMalloc(&d_rows, 8ull * rows_words));
	CK(cudaMalloc(&d_ctl, 8ull * C_NCTL));
	CK(cudaMalloc(&d_eexist, 8ull * nkeys));
	CK(cudaMalloc(&d_e2big, 8ull * nkeys));
	CK(cudaMalloc(&d_unstable, 8ull * nkeys));
	CK(cudaMalloc(&d_rstat, 8ull * R_NSTAT));
	CK(cudaMalloc(&d_out, (size_t)launches * n_threads));
	cudaStream_t sA, sB;
	CK(cudaStreamCreateWithFlags(&sA, cudaStreamNonBlocking));
	CK(cudaStreamCreateWithFlags(&sB, cudaStreamNonBlocking));

	std::vector<u64> dir(S), rows(rows_words), ctl(C_NCTL), eex(nkeys), e2b(nkeys), uns(nkeys),
		rst(R_NSTAT);
	std::vector<unsigned char> outc((size_t)launches * n_threads);
	Agg G;

	Params P0;
	memset(&P0, 0, sizeof P0);
	P0.dir = (u64)d_dir;
	P0.rows = (u64)d_rows;
	P0.eexist = (u64)d_eexist;
	P0.e2big = (u64)d_e2big;
	P0.unstable = (u64)d_unstable;
	P0.outcomes = (u64)d_out;
	P0.rstat = (u64)d_rstat;
	P0.S = S;
	P0.logS = logS;
	P0.max_entries = max_entries;
	P0.rows_cap = rows_cap;
	P0.nkeys = nkeys;

	// Warm-up: one k_ops launch with no work threads, on scratch control words
	{
		u64 *w_ctl;
		CK(cudaMalloc(&w_ctl, 8ull * C_NCTL));
		CK(cudaMemset(w_ctl, 0, 8ull * C_NCTL));
		Params W = P0;
		W.ctl = (u64)w_ctl;
		W.n_threads = 0;
		k_ops<<<grid, block, 0, sA>>>(W);
		CK(cudaGetLastError());
		CK(cudaDeviceSynchronize());
		CK(cudaFree(w_ctl));
	}

	const u64 PREFILL_V = 5;
	for (u32 tr = 0; tr < trials; tr++) {
		u64 seed = tr == 0 ? seed0 : splitmix64(seed0 + tr);
		CK(cudaMemset(d_dir, 0, 8ull * S));
		CK(cudaMemset(d_rows, 0, 8ull * rows_words));
		CK(cudaMemset(d_ctl, 0, 8ull * C_NCTL));
		CK(cudaMemset(d_eexist, 0, 8ull * nkeys));
		CK(cudaMemset(d_e2big, 0, 8ull * nkeys));
		CK(cudaMemset(d_unstable, 0, 8ull * nkeys));
		CK(cudaMemset(d_rstat, 0, 8ull * R_NSTAT));
		CK(cudaMemset(d_out, 0, (size_t)launches * n_threads));
		u64 ones = ~0ull;
		CK(cudaMemcpy(d_ctl + C_FIRST_START_NS, &ones, 8, cudaMemcpyHostToDevice));
		CK(cudaMemcpy(d_rstat + R_START_NS, &ones, 8, cudaMemcpyHostToDevice));
		CK(cudaMemcpy(d_rstat + R_FIRST_TRANSITION_SUM_MIN, &ones, 8, cudaMemcpyHostToDevice));

		// keys inserted beforehand (the agent's copy): key j = 4p, live in row p,
		// its value in shard 0; the count starts at prefill
		std::vector<char> prefilled(nkeys, 0);
		if (prefill) {
			std::vector<u64> hdir(S, 0);
			for (u32 p = 0; p < prefill; p++) {
				u32 j = (4 * p) % nkeys;
				u32 K = key_of(j);
				u32 i = home_slot(K, seed, logS);
				while (hdir[i])
					i = (i + 1) & (S - 1);
				hdir[i] = ((u64)K << 32) | ((u64)p << 3) | ST_LIVE;
				u64 v = PREFILL_V;
				CK(cudaMemcpy(d_rows + (size_t)p * NSHARDS, &v, 8, cudaMemcpyHostToDevice));
				prefilled[j] = 1;
			}
			CK(cudaMemcpy(d_dir, hdir.data(), 8ull * S, cudaMemcpyHostToDevice));
			u64 n = prefill;
			CK(cudaMemcpy(d_ctl + C_BOUND, &n, 8, cudaMemcpyHostToDevice));
			CK(cudaMemcpy(d_ctl + C_NEXT_ROW, &n, 8, cudaMemcpyHostToDevice));
		}
		CK(cudaDeviceSynchronize());

		Params P = P0;
		P.ctl = (u64)d_ctl;
		P.seed = seed;
		P.n_threads = n_threads;
		P.launch = 0;
		u64 total_blocks = (u64)grid * launches;
		if (reader) {
			u32 rthreads = 32, rblocks = (S + rthreads - 1) / rthreads;
			k_reader<<<rblocks, rthreads, 0, sB>>>(P, total_blocks, 20000000000ull);
			CK(cudaGetLastError());
		}
		for (u32 l = 0; l < launches; l++) {
			P.launch = l;
			k_ops<<<grid, block, 0, sA>>>(P);
			CK(cudaGetLastError());
		}
		CK(cudaDeviceSynchronize());
		CK(cudaMemcpy(dir.data(), d_dir, 8ull * S, cudaMemcpyDeviceToHost));
		CK(cudaMemcpy(rows.data(), d_rows, 8ull * rows_words, cudaMemcpyDeviceToHost));
		CK(cudaMemcpy(ctl.data(), d_ctl, 8ull * C_NCTL, cudaMemcpyDeviceToHost));
		CK(cudaMemcpy(eex.data(), d_eexist, 8ull * nkeys, cudaMemcpyDeviceToHost));
		CK(cudaMemcpy(e2b.data(), d_e2big, 8ull * nkeys, cudaMemcpyDeviceToHost));
		CK(cudaMemcpy(uns.data(), d_unstable, 8ull * nkeys, cudaMemcpyDeviceToHost));
		CK(cudaMemcpy(rst.data(), d_rstat, 8ull * R_NSTAT, cudaMemcpyDeviceToHost));
		CK(cudaMemcpy(outc.data(), d_out, (size_t)launches * n_threads, cudaMemcpyDeviceToHost));

		// ------------------------------------------------ checks
		u64 V[V_N] = { 0 };
		std::vector<std::string> notes;
		auto note = [&](const char *fmt, ...) {
			if (notes.size() >= 24)
				return;
			char buf[256];
			va_list ap;
			va_start(ap, fmt);
			vsnprintf(buf, sizeof buf, fmt, ap);
			va_end(ap);
			notes.push_back(buf);
		};
		auto jidx = [&](u32 K) -> int {
			u32 j = (K - 1000u) * INV;
			return (j < nkeys && key_of(j) == K) ? (int)j : -1;
		};
		std::vector<int> nslots(nkeys, 0), slot_of(nkeys, -1);
		std::vector<char> live_of(nkeys, 0), refused_of(nkeys, 0);
		std::vector<u64> row_of(nkeys, ~0ull), delivered(nkeys, 0);
		u64 nonempty = 0, nclaiming = 0, nlive = 0, nrefused = 0;
		std::vector<int> row_used(rows_cap, 0);
		for (u32 s = 0; s < S; s++) {
			u64 h = dir[s];
			u64 st = h & 7;
			if (st == ST_EMPTY) {
				if (h) {
					V[V_GARBAGE]++;
					note("slot %u: empty state with bits 0x%llx", s, h);
				}
				continue;
			}
			nonempty++;
			u32 K = (u32)(h >> 32);
			int j = jidx(K);
			if (st != ST_CLAIMING && st != ST_LIVE && st != ST_REFUSED) {
				V[V_GARBAGE]++;
				note("slot %u: state %llu", s, st);
				continue;
			}
			if (st == ST_REFUSED && (h & ~((0xFFFFFFFFull << 32) | 7ull))) {
				V[V_GARBAGE]++;
				note("slot %u: refused header with row bits 0x%llx", s, h);
			}
			if (j < 0) {
				V[V_GARBAGE]++;
				note("slot %u: unknown key %u", s, K);
				continue;
			}
			nslots[j]++;
			if (nslots[j] == 2) {
				V[V_TWO_SLOTS]++;
				note("key j=%d K=%u in slots %d and %u", j, K, slot_of[j], s);
			}
			slot_of[j] = slot_of[j] < 0 ? (int)s : slot_of[j];
			u32 i = home_slot(K, seed, logS);
			while (i != s) {
				if ((dir[i] & 7) == ST_EMPTY) {
					V[V_CHAIN]++;
					note("key j=%d slot %u: empty slot %u on its probe chain", j, s, i);
					break;
				}
				i = (i + 1) & (S - 1);
			}
			if (st == ST_CLAIMING) {
				nclaiming++;
				continue;
			}
			if (st == ST_REFUSED) {
				nrefused++;
				refused_of[j] = 1;
				continue;
			}
			nlive++;
			live_of[j] = 1;
			u64 row = (h >> 3) & 0x1FFFFFFFull;
			if (row >= rows_cap) {
				V[V_GARBAGE]++;
				note("slot %u key j=%d: row %llu beyond rows_cap", s, j, row);
				continue;
			}
			if (row >= max_entries) {
				V[V_ROW_RANGE]++;
				note("slot %u key j=%d: row %llu >= max_entries", s, j, row);
			}
			if (row_used[row]++) {
				V[V_ROW_DUP]++;
				note("row %llu in two live headers", row);
			}
			row_of[j] = row;
			u64 sum = 0;
			for (int k = 0; k < NSHARDS; k++)
				sum += rows[row * NSHARDS + k];
			delivered[j] += sum;
		}
		if (nlive > max_entries) {
			V[V_LIVE_GT_MAX]++;
			note("live slots %llu > max_entries %u", nlive, max_entries);
		}
		if (nlive != expect_live) {
			V[V_LIVE_COUNT]++;
			note("live slots %llu != min(keys %llu, max_entries %u)", nlive, keys_used, max_entries);
		}
		if (nclaiming) {
			V[V_LEFT_CLAIMING]++;
			note("%llu slots left claiming", nclaiming);
		}
		u64 all_rows = 0, ref_rows = 0;
		for (u32 r = 0; r < rows_cap; r++) {
			u64 sum = 0;
			for (int k = 0; k < NSHARDS; k++)
				sum += rows[(size_t)r * NSHARDS + k];
			all_rows += sum;
			if (row_used[r])
				ref_rows += sum;
		}
		if (all_rows != ref_rows) {
			V[V_ORPHAN]++;
			note("orphan adds: all rows %llu, rows behind live headers %llu", all_rows, ref_rows);
		}
		// bookkeeping: every winner adds once to the count (v2), and
		// refused slots are exactly the winners that published refused
		if (FAULT == 0 || FAULT == 2 || FAULT == 5) {
			if (ctl[C_BOUND] != nlive + nrefused) {
				V[V_BOOKKEEPING]++;
				note("count %llu != live %llu + refused %llu", ctl[C_BOUND], nlive, nrefused);
			}
			if (ctl[C_REFUSED_PUBLISHED] != nrefused) {
				V[V_BOOKKEEPING]++;
				note("refused published %llu != refused slots %llu", ctl[C_REFUSED_PUBLISHED], nrefused);
			}
		}
		if (FAULT == 6 && ctl[C_NEXT_ROW] != nlive) {
			V[V_BOOKKEEPING]++;
			note("next_row %llu != live headers %llu", ctl[C_NEXT_ROW], nlive);
		}
		// outcome tallies
		std::vector<std::array<u64, 9>> T(nkeys);
		for (auto &a : T)
			a.fill(0);
		u64 then_null = 0, then_null_unstable = 0;
		std::vector<u64> L_eex(launches, 0), L_e2b(launches, 0), L_won(launches, 0), L_uns(launches, 0);
		for (u32 l = 0; l < launches; l++)
			for (u32 t = 0; t < n_threads; t++) {
				unsigned char oc = outc[(size_t)l * n_threads + t];
				u32 j = keyidx_of_tid(t, nkeys, n_threads);
				int b = oc & 0xF;
				if (b == OC_EEXIST_LIVE || b == OC_EEXIST_WAITED || b == OC_EEXIST_UNSTABLE)
					L_eex[l]++;
				if (b == OC_EEXIST_UNSTABLE)
					L_uns[l]++;
				if (b == OC_WON)
					L_won[l]++;
				if (b == OC_E2BIG)
					L_e2b[l]++;
				if (oc & OC_FLAG_THEN_NULL) {
					if (b == OC_EEXIST_UNSTABLE)
						then_null_unstable++;
					else
						then_null++;
				}
				if (b > 8)
					b = 0;
				T[j][b]++;
			}
		if (then_null) {
			V[V_EEXIST_THEN_NULL]++;
			note("EEXIST (not unstable), then own lookup NULL: %llu", then_null);
		}
		if (ctl[C_ROW_OVERFLOW]) {
			V[V_ANOM]++;
			note("row overflow %llu", ctl[C_ROW_OVERFLOW]);
		}
		if (ctl[C_KEY_CHANGED]) {
			V[V_KEY_CHANGED]++;
			note("waited-on slot changed key %llu times", ctl[C_KEY_CHANGED]);
		}
		u64 tot_att = 0, tot_eex = 0, tot_e2b = 0, tot_uns = 0, tot_del = 0, tot_pref = 0;
		u64 n_key_sum = 0, n_e2b_live = 0, n_eex_not_live = 0;
		for (u32 j = 0; j < nkeys; j++) {
			u64 pv = prefilled[j] ? PREFILL_V : 0;
			u64 expect = pv + attempts[j] - eex[j] - e2b[j];
			tot_att += attempts[j];
			tot_eex += eex[j];
			tot_e2b += e2b[j];
			tot_uns += uns[j];
			tot_del += delivered[j];
			tot_pref += pv;
			if (delivered[j] != expect) {
				V[V_KEY_SUM]++;
				if (n_key_sum++ < 4)
					note("key j=%u K=%u: delivered %llu != %llu (attempts %llu - EEXIST %llu - E2BIG %llu + prefill %llu)",
					     j, key_of(j), delivered[j], expect, attempts[j], eex[j], e2b[j], pv);
			}
			u64 t_eex = T[j][OC_EEXIST_LIVE] + T[j][OC_EEXIST_WAITED] + T[j][OC_EEXIST_UNSTABLE];
			if (t_eex != eex[j] || T[j][OC_E2BIG] != e2b[j] || T[j][OC_EEXIST_UNSTABLE] != uns[j] ||
			    T[j][OC_NONE] != 0) {
				V[V_TALLY]++;
				note("key j=%u: tallies eexist %llu/%llu e2big %llu/%llu unstable %llu/%llu none %llu", j,
				     t_eex, eex[j], T[j][OC_E2BIG], e2b[j], T[j][OC_EEXIST_UNSTABLE], uns[j],
				     T[j][OC_NONE]);
			}
			if (pv + T[j][OC_HIT] + T[j][OC_WON] != delivered[j] && nslots[j] <= 1) {
				V[V_TALLY]++;
				note("key j=%u: hits %llu + won %llu + prefill %llu != delivered %llu", j, T[j][OC_HIT],
				     T[j][OC_WON], pv, delivered[j]);
			}
			u64 want_won = (live_of[j] && !prefilled[j]) ? 1 : 0;
			if (T[j][OC_WON] != want_won) {
				V[V_WINNERS]++;
				note("key j=%u: %llu winners (live %d, refused %d, prefilled %d)", j, T[j][OC_WON],
				     live_of[j], refused_of[j], prefilled[j]);
			}
			if (e2b[j] && live_of[j]) {
				V[V_E2BIG_LIVE_KEY]++;
				if (n_e2b_live++ < 4)
					note("key j=%u: E2BIG %llu but the key is live", j, e2b[j]);
			}
			if (eex[j] > uns[j] && !live_of[j]) {
				V[V_EEXIST_NOT_LIVE]++;
				if (n_eex_not_live++ < 4)
					note("key j=%u: EEXIST %llu (unstable %llu) but the key is not live", j, eex[j],
					     uns[j]);
			}
		}
		if (tot_e2b && (nlive < max_entries || keys_used <= max_entries)) {
			V[V_E2BIG_WITH_ROOM]++;
			note("E2BIG %llu with live %llu, max_entries %u, keys %llu", tot_e2b, nlive, max_entries,
			     keys_used);
		}
		if (tot_del != tot_pref + tot_att - tot_eex - tot_e2b) {
			V[V_TOTAL_SUM]++;
			note("total delivered %llu != %llu", tot_del, tot_pref + tot_att - tot_eex - tot_e2b);
		}
		if (rst[R_ZERO_SUM]) {
			V[V_READER_ZERO]++;
			note("reader: %llu live headers with row sum 0", rst[R_ZERO_SUM]);
		}
		if (rst[R_DECREASE]) {
			V[V_READER_DECREASE]++;
			note("reader: %llu row sums went down", rst[R_DECREASE]);
		}

		// directory shape (live and refused slots)
		u64 off_home = 0, maxd = 0;
		for (u32 j = 0; j < nkeys; j++)
			if (slot_of[j] >= 0) {
				u32 h = home_slot(key_of(j), seed, logS);
				u64 d = ((u32)slot_of[j] - h) & (S - 1);
				if (d)
					off_home++;
				maxd = d > maxd ? d : maxd;
			}

		u64 nv = 0;
		for (int v = 0; v < V_N; v++) {
			nv += V[v];
			G.viol[v] += V[v];
		}
		G.trials++;
		if (nv)
			G.trials_with_violation++;
		G.e2big += tot_e2b;
		G.e2big_tmin = tot_e2b < G.e2big_tmin ? tot_e2b : G.e2big_tmin;
		G.e2big_tmax = tot_e2b > G.e2big_tmax ? tot_e2b : G.e2big_tmax;
		G.eexist_tmin = tot_eex < G.eexist_tmin ? tot_eex : G.eexist_tmin;
		G.eexist_tmax = tot_eex > G.eexist_tmax ? tot_eex : G.eexist_tmax;
		G.eexist += tot_eex;
		G.unstable += tot_uns;
		G.unstable_then_null += then_null_unstable;
		G.eexist_then_null += then_null;
		G.waits += ctl[C_WAITS];
		G.wait_sleeps += ctl[C_WAIT_SLEEPS];
		G.wait_max_ns = ctl[C_WAIT_MAX_NS] > G.wait_max_ns ? ctl[C_WAIT_MAX_NS] : G.wait_max_ns;
		G.wait_max_req = ctl[C_WAIT_MAX_REQ_NS] > G.wait_max_req ? ctl[C_WAIT_MAX_REQ_NS] : G.wait_max_req;
		G.lost_same += ctl[C_LOST_SAME];
		G.lost_other += ctl[C_LOST_OTHER];
		G.walk_saw_claiming += ctl[C_WALK_SAW_CLAIMING];
		G.giveback += ctl[C_GIVEBACK];
		G.e2big_pre += ctl[C_E2BIG_PRECHECK];
		G.e2big_after += ctl[C_E2BIG_AFTER_ADD];
		G.e2big_saw_refused += ctl[C_E2BIG_SAW_REFUSED];
		G.precheck_taken += ctl[C_PRECHECK_SLOT_TAKEN];
		G.e2big_fulldir += ctl[C_E2BIG_FULLDIR];
		G.fulldir_unstable += ctl[C_FULLDIR_UNSTABLE];
		G.insert_fullwalk += ctl[C_WALK_OVERFLOW];
		G.lookup_fullwalk += ctl[C_LOOKUP_FULLWALK];
		G.refused_sum += nrefused;
		G.refused_max = nrefused > G.refused_max ? nrefused : G.refused_max;
		G.live_min = nlive < G.live_min ? nlive : G.live_min;
		G.live_max = nlive > G.live_max ? nlive : G.live_max;
		G.keys_off_home += off_home;
		G.max_dist = maxd > G.max_dist ? maxd : G.max_dist;
		bool overlap = false;
		if (reader) {
			G.r_polls += rst[R_POLLS];
			G.r_claiming += rst[R_SEEN_CLAIMING];
			G.r_transitions += rst[R_TRANSITIONS];
			G.r_live += rst[R_LIVE_CHECKS];
			G.r_zero += rst[R_ZERO_SUM];
			G.r_decr += rst[R_DECREASE];
			G.r_changes += rst[R_SUM_CHANGES];
			G.r_timeout += rst[R_TIMED_OUT];
			if (rst[R_FIRST_TRANSITION_SUM_MIN] < G.r_min_tr_sum)
				G.r_min_tr_sum = rst[R_FIRST_TRANSITION_SUM_MIN];
			overlap = rst[R_START_NS] < ctl[C_FIRST_START_NS] &&
				  (rst[R_SEEN_CLAIMING] || rst[R_TRANSITIONS]);
			if (overlap)
				G.r_overlap_trials++;
		}

		bool full = detail && (tr == 0 || (nv && G.trials_with_violation <= 3));
		if (full) {
			printf("\n=== trial %u seed=0x%016llx ===\n", tr, seed);
			printf("dir: nonempty=%llu live=%llu refused=%llu claiming=%llu (max_entries %u, S %u, expected live %llu) count=%llu\n",
			       nonempty, nlive, nrefused, nclaiming, max_entries, S, expect_live, ctl[C_BOUND]);
			printf("dir shape: keys off home=%llu max probe distance=%llu\n", off_home, maxd);
			printf("totals: attempts=%llu prefill=%llu delivered=%llu EEXIST=%llu E2BIG=%llu unstable=%llu "
			       "| all rows=%llu rows behind live headers=%llu\n",
			       tot_att, tot_pref, tot_del, tot_eex, tot_e2b, tot_uns, all_rows, ref_rows);
			printf("branches: lost_same_key=%llu lost_other_key=%llu walk_saw_claiming=%llu waits=%llu "
			       "wait_sleeps=%llu wait_max_ns=%llu givebacks=%llu e2big_precheck=%llu precheck_slot_taken=%llu "
			       "e2big_refused_winner=%llu e2big_saw_refused=%llu e2big_fulldir=%llu fulldir_unstable=%llu "
			       "insert_fullwalk=%llu lookup_fullwalk=%llu eexist_then_null=%llu unstable_then_null=%llu "
			       "row_overflow=%llu key_changed=%llu\n",
			       ctl[C_LOST_SAME], ctl[C_LOST_OTHER], ctl[C_WALK_SAW_CLAIMING], ctl[C_WAITS],
			       ctl[C_WAIT_SLEEPS], ctl[C_WAIT_MAX_NS], ctl[C_GIVEBACK], ctl[C_E2BIG_PRECHECK],
			       ctl[C_PRECHECK_SLOT_TAKEN], ctl[C_E2BIG_AFTER_ADD], ctl[C_E2BIG_SAW_REFUSED],
			       ctl[C_E2BIG_FULLDIR], ctl[C_FULLDIR_UNSTABLE], ctl[C_WALK_OVERFLOW],
			       ctl[C_LOOKUP_FULLWALK], ctl[C_EEXIST_THEN_NULL], ctl[C_UNSTABLE_THEN_NULL],
			       ctl[C_ROW_OVERFLOW], ctl[C_KEY_CHANGED]);
			printf("kernel span: first block start -> last block end = %.3f ms (%u launches)\n",
			       (ctl[C_LAST_END_NS] - ctl[C_FIRST_START_NS]) / 1e6, launches);
			if (reader)
				printf("reader: start_before_inserters=%d polls=%llu seen_claiming=%llu transitions_seen=%llu "
				       "min_sum_at_transition=%lld live_checks=%llu zero_sum=%llu decrease=%llu "
				       "sum_changes=%llu timed_out=%llu\n",
				       rst[R_START_NS] < ctl[C_FIRST_START_NS], rst[R_POLLS], rst[R_SEEN_CLAIMING],
				       rst[R_TRANSITIONS],
				       rst[R_FIRST_TRANSITION_SUM_MIN] == ~0ull ? -1LL
										 : (long long)rst[R_FIRST_TRANSITION_SUM_MIN],
				       rst[R_LIVE_CHECKS], rst[R_ZERO_SUM], rst[R_DECREASE], rst[R_SUM_CHANGES],
				       rst[R_TIMED_OUT]);
			printf("per launch (EEXIST/E2BIG/won/unstable):");
			for (u32 l = 0; l < launches; l++)
				printf(" L%u %llu/%llu/%llu/%llu", l + 1, L_eex[l], L_e2b[l], L_won[l], L_uns[l]);
			printf("\nlongest wait requested_ns=%llu\n", ctl[C_WAIT_MAX_REQ_NS]);
			if (nkeys <= 64) {
				printf("per key: j K slots slot home dist row state attempts delivered EEXIST(live/waited/unstable) E2BIG hits won ok\n");
				for (u32 j = 0; j < nkeys; j++) {
					u32 h = home_slot(key_of(j), seed, logS);
					long d = slot_of[j] >= 0 ? (long)(((u32)slot_of[j] - h) & (S - 1)) : -1;
					u64 pv = prefilled[j] ? PREFILL_V : 0;
					bool ok = delivered[j] == pv + attempts[j] - eex[j] - e2b[j];
					printf("  %2u %10u %d %4d %4u %3ld %4lld %s %7llu %7llu %6llu(%llu/%llu/%llu) %7llu %7llu %llu %s%s\n",
					       j, key_of(j), nslots[j], slot_of[j], h, d,
					       row_of[j] == ~0ull ? -1LL : (long long)row_of[j],
					       live_of[j] ? "live" : refused_of[j] ? "refused" : "none", attempts[j],
					       delivered[j], eex[j], T[j][OC_EEXIST_LIVE], T[j][OC_EEXIST_WAITED],
					       T[j][OC_EEXIST_UNSTABLE], e2b[j], T[j][OC_HIT], T[j][OC_WON],
					       ok ? "ok" : "MISMATCH", prefilled[j] ? " (prefilled)" : "");
				}
			}
			printf("violations:");
			for (int v = 0; v < V_N; v++)
				if (V[v])
					printf(" %s=%llu", vname[v], V[v]);
			printf(nv ? "\n" : " none\n");
			for (auto &s : notes)
				printf("  note: %s\n", s.c_str());
		} else {
			printf("trial %u seed=0x%016llx live=%llu refused=%llu claiming=%llu EEXIST=%llu E2BIG=%llu unstable=%llu "
			       "lost_same=%llu lost_other=%llu waits=%llu precheck_taken=%llu fulldir=%llu/%llu maxd=%llu%s viol=%llu\n",
			       tr, seed, nlive, nrefused, nclaiming, tot_eex, tot_e2b, tot_uns, ctl[C_LOST_SAME],
			       ctl[C_LOST_OTHER], ctl[C_WAITS], ctl[C_PRECHECK_SLOT_TAKEN], ctl[C_E2BIG_FULLDIR],
			       ctl[C_FULLDIR_UNSTABLE], maxd,
			       reader ? (overlap ? " reader_overlap=1" : " reader_overlap=0") : "", nv);
			if (nv)
				for (auto &s : notes)
					printf("  note: %s\n", s.c_str());
		}
		fflush(stdout);
	}

	printf("\n=== summary build=%s max_entries=%u keys=%u trials=%u ===\n", fault_name(), max_entries, nkeys,
	       trials);
	printf("trials_with_violation=%llu/%llu\n", G.trials_with_violation, G.trials);
	printf("violations by kind:");
	u64 any = 0;
	for (int v = 0; v < V_N; v++)
		if (G.viol[v]) {
			printf(" %s=%llu", vname[v], G.viol[v]);
			any++;
		}
	printf(any ? "\n" : " none\n");
	printf("counted: EEXIST=%llu E2BIG=%llu unstable=%llu eexist_then_null=%llu unstable_then_null=%llu\n",
	       G.eexist, G.e2big, G.unstable, G.eexist_then_null, G.unstable_then_null);
	printf("per trial: EEXIST min=%llu max=%llu, E2BIG min=%llu max=%llu, live min=%llu max=%llu (expected %llu)\n",
	       G.eexist_tmin, G.eexist_tmax, G.e2big_tmin, G.e2big_tmax, G.live_min, G.live_max, expect_live);
	printf("refused slots: sum=%llu max per trial=%llu\n", G.refused_sum, G.refused_max);
	printf("branches: lost_same_key=%llu lost_other_key=%llu walk_saw_claiming=%llu waits=%llu wait_sleeps=%llu "
	       "givebacks=%llu e2big_precheck=%llu precheck_slot_taken=%llu e2big_refused_winner=%llu "
	       "e2big_saw_refused=%llu e2big_fulldir=%llu fulldir_unstable=%llu insert_fullwalk=%llu lookup_fullwalk=%llu\n",
	       G.lost_same, G.lost_other, G.walk_saw_claiming, G.waits, G.wait_sleeps, G.giveback, G.e2big_pre,
	       G.precheck_taken, G.e2big_after, G.e2big_saw_refused, G.e2big_fulldir, G.fulldir_unstable,
	       G.insert_fullwalk, G.lookup_fullwalk);
	printf("longest wait: elapsed_ns=%llu requested_ns=%llu (bound %u)\n", G.wait_max_ns, G.wait_max_req,
	       WAIT_BOUND_NS);
	printf("dir: keys off home (sum over trials)=%llu max probe distance=%llu\n", G.keys_off_home, G.max_dist);
	if (reader)
		printf("reader: trials_with_overlap=%llu polls=%llu seen_claiming=%llu transitions_seen=%llu "
		       "live_checks=%llu zero_sum=%llu decrease=%llu sum_changes=%llu timed_out=%llu "
		       "min_sum_at_transition_over_trials=%lld\n",
		       G.r_overlap_trials, G.r_polls, G.r_claiming, G.r_transitions, G.r_live, G.r_zero, G.r_decr,
		       G.r_changes, G.r_timeout, G.r_min_tr_sum == ~0ull ? -1LL : (long long)G.r_min_tr_sum);
	printf("RESULT %s\n", G.trials_with_violation ? "VIOLATIONS" : "CLEAN");
	return G.trials_with_violation ? 1 : 0;
}

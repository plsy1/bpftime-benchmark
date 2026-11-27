// Test kernels for the device hash map's GPU functions (tests only: shipped
// as ../devhash_test_ptx.h, one whole module per build of devhash.cu). A port
// of round two's stress kernels (results gpu/hash-round2-20261127,
// c5/src/c5.cu: k_ops, k_reader) onto the shipped functions and the block
// layout of ../trampoline/gpu_devhash_layout.h.
//
// devhash_k_ops: each thread takes its key, looks it up, adds 1 through the
// returned pointer on a hit, and otherwise inserts with BPF_NOEXIST (value
// 1); per key index it counts EEXIST, E2BIG and unstable EEXIST, and checks
// that a thread that got EEXIST then finds the key with its own lookup.
// devhash_k_reader: thread t watches slot t while k_ops runs; a live header
// read with ld.acquire must have a row sum of at least 1.
#include "devhash.cu"
// as CUDA's headers define it (built here without them)
#define __launch_bounds__(n) __attribute__((launch_bounds(n)))

namespace devhash_harness
{
using namespace devhash_impl;

struct params {
	u64 block; // the device block (generic address)
	u64 max_entries, shards;
	u64 hctl; // harness counters, H_*
	u64 eexist, e2big, unstable; // per key index
	u64 outcomes; // this launch's outcome bytes, n_threads of them
	u32 n_threads, nkeys;
	u32 key_mul, key_add; // key of index j: j * key_mul + key_add
};

enum : u32 {
	H_BLOCKS_DONE = 0,
	H_EEXIST_THEN_NULL,
	H_UNSTABLE_THEN_NULL,
	H_N
};

// reader statistics, after the harness counters
enum : u32 {
	R_POLLS = H_N,
	R_LIVE_CHECKS,
	R_ZERO_SUM,
	R_DECREASE,
	R_TRANSITIONS,
	R_TIMED_OUT,
	R_TRANSITION_SUM_MIN, // host sets ~0
	R_END
};

static __device__ __forceinline__ u32 key_of_index(const params &p, u32 j)
{
	return j * p.key_mul + p.key_add;
}
// one key per thread when nkeys == n_threads, else round two's scrambling
static __device__ __forceinline__ u32 keyidx_of_tid(u32 tid, u32 nkeys,
						    u32 n_threads)
{
	if (nkeys == n_threads)
		return tid;
	return (u32)(((u64)(tid * 2654435761u) * nkeys) >> 32);
}
static __device__ __forceinline__ void hadd(const params &p, u32 i, u64 v)
{
	red_add(to_global(p.hctl) + 8ull * i, v);
}
static __device__ __forceinline__ void hmin(const params &p, u32 i, u64 v)
{
	asm volatile("red.relaxed.gpu.global.min.u64 [%0], %1;" ::"l"(
			     to_global(p.hctl) + 8ull * i),
		     "l"(v)
		     : "memory");
}
} // namespace devhash_harness

using namespace devhash_harness;

extern "C" __global__ void __launch_bounds__(256)
	devhash_k_ops(params p)
{
	const u32 tid = blockIdx.x * blockDim.x + threadIdx.x;
	if (tid < p.n_threads) {
		const u32 j = keyidx_of_tid(tid, p.nkeys, p.n_threads);
		const u32 key = key_of_index(p, j);
		volatile unsigned char *oc =
			(volatile unsigned char *)(p.outcomes + tid);
		const u64 v = __bpftime_devhash_lookup(p.block, key,
						       p.max_entries, p.shards);
		if (v) {
			red_add(to_global(v), 1);
			*oc = OC_HIT;
		} else {
			u64 one = 1;
			const u64 r = __bpftime_devhash_insert(
				p.block, key, (u64)&one, NOEXIST, p.max_entries,
				p.shards);
#ifdef BPFTIME_DEVHASH_TEST
			u32 code = *oc; // stored by the insert
#else
			// the shipped build has no outcome channel
			u32 code = r == 0		 ? (u32)OC_WON :
				   r == R_EEXIST ? (u32)OC_EEXIST_LIVE :
				   r == R_E2BIG	 ? (u32)OC_E2BIG :
						   (u32)OC_NONE;
			*oc = (unsigned char)code;
#endif
			if (r == R_EEXIST) {
				red_add(to_global(p.eexist) + 8ull * j, 1);
				if (code == OC_EEXIST_UNSTABLE)
					red_add(to_global(p.unstable) + 8ull * j,
						1);
				// "EEXIST, then not NULL": the thread's own
				// lookup after EEXIST
				if (__bpftime_devhash_lookup(p.block, key,
							     p.max_entries,
							     p.shards) == 0) {
					*oc = (unsigned char)(code |
							      OC_FLAG_THEN_NULL);
					hadd(p,
					     code == OC_EEXIST_UNSTABLE ?
						     H_UNSTABLE_THEN_NULL :
						     H_EEXIST_THEN_NULL,
					     1);
				}
			} else if (r == R_E2BIG) {
				red_add(to_global(p.e2big) + 8ull * j, 1);
			}
		}
	}
	__syncthreads();
	if (threadIdx.x == 0)
		hadd(p, H_BLOCKS_DONE, 1);
}

extern "C" __global__ void devhash_k_reader(params p, u64 total_blocks,
					    u64 tmax_ns)
{
	const layout l = layout_for(p.max_entries, p.shards);
	const u64 g = to_global(p.block);
	const u32 t = blockIdx.x * blockDim.x + threadIdx.x;
	if (t >= l.slots)
		return;
	const u64 a = g + slot_offset(l, t);
	const u64 t0 = gtimer();
	u64 polls = 0, live_checks = 0, zero_sum = 0, decrease = 0,
	    transitions = 0, timed_out = 0, tr_min = ~0ull, last_sum = 0;
	bool live_seen = false;
	u64 last_state = ST_EMPTY;
	for (;;) {
		if (ld_relaxed(to_global(p.hctl) + 8ull * H_BLOCKS_DONE) >=
		    total_blocks)
			break;
		if (gtimer() - t0 > tmax_ns) {
			timed_out = 1;
			break;
		}
		const u64 h = ld_acquire(a); // the snapshot's form
		polls++;
		if (state_of(h) == ST_LIVE && row_of(h) < l.max_entries) {
			const u64 row = row_of(h);
			u64 sum = 0;
			if (row < l.r) {
				for (u64 s = 0; s < l.shards; s++)
					sum += ld_relaxed(
						g + value_offset(l, row, (u32)s));
			} else {
				sum = ld_relaxed(g + value_offset(l, row, 0));
			}
			live_checks++;
			if (sum == 0)
				zero_sum++;
			if (!live_seen && last_state != ST_LIVE && polls > 1) {
				transitions++;
				tr_min = sum < tr_min ? sum : tr_min;
			}
			if (live_seen && sum < last_sum)
				decrease++;
			last_sum = sum;
			live_seen = true;
		}
		last_state = state_of(h);
	}
	hadd(p, R_POLLS, polls);
	hadd(p, R_LIVE_CHECKS, live_checks);
	hadd(p, R_ZERO_SUM, zero_sum);
	hadd(p, R_DECREASE, decrease);
	hadd(p, R_TRANSITIONS, transitions);
	hadd(p, R_TIMED_OUT, timed_out);
	hmin(p, R_TRANSITION_SUM_MIN, tr_min);
}

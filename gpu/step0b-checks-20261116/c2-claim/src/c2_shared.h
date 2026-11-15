// c2_shared.h: layouts shared by the device code (c2.cuh, kernels.cu) and the host programs
// (c2chk.cpp, c2bench.cpp). Plain integer types only, so g++ and nvcc agree on the layout.
#pragma once
#include <stdint.h>

#define C2_NONE 0xffffffffu
#define C2_HDR_WORDS 4u   // per-slot header (32 B): [0] live = gen << 32 | lane count, [1] relgen
                          // (first-exit control), [2] ledger (checker), [3] spare
#define C2_CTR_STRIDE 16u // counters 128 B apart
#define C2_LANES 32u
#define C2_HEAD_STRIDE 16u // stack heads 128 B apart

// pool kinds (template parameter P)
#define C2_BMSM 0      // bitmap, one region per SM (chosen by %smid), first try = %warpid's bit
#define C2_BMGLOB 1    // bitmap, one region, first try from a hash of the logical warp index
#define C2_STKSM 2     // lock-free stack with a version tag, one per SM (chosen by %smid)
#define C2_STKGLOB 3   // lock-free stack with a version tag, one for the GPU
#define C2_STKSM_NV 4  // control: per-SM stack WITHOUT the version tag
#define C2_STKGLOB_NV 5 // control: global stack WITHOUT the version tag
// per-lane claims (outside the task's stated mechanism): every thread claims its own slot,
// no warp collective is involved; the bitmap is lane-major so a warp's lanes hit distinct words
#define C2_LNSM 6      // one region per SM (chosen by %smid), first try = (%warpid, %laneid)'s bit
#define C2_LNGLOB 7    // one region, first try from (hash of the logical warp index, %laneid)
// per-lane claims on one 32-bit flag word per thread slot (no bitmap word shared by threads):
// claim = atom.cas 0 -> 1 (acquire in ORD), release = st.release 0
#define C2_LFSM 8      // regions per SM, first try = the (%smid, %warpid, %laneid) slot
#define C2_LFGLOB 9    // one region, first try = (hash of the logical warp index, %laneid)

// exit kinds (template parameter X)
#define C2_X_LANE 0  // the design: every lane decrements the slot's count; the lane that makes it 0 releases
#define C2_X_AGG 1   // not a decision variant: one decrement per exit group (match_any + __syncwarp)
#define C2_X_FIRST 2 // control: the first exit call of a claim releases the slot (per-lane
                     // pools: the slot is released right after the claim, then used)
#define C2_X_CMASK 3 // warp claim with the mask constructed from blockDim (the lanes that exist in
                     // the warp, CUDA guide 3.2.2.1) instead of __activemask(); valid only where every
                     // thread of the warp calls the helper (a kernel's first statement); per-lane exit
#define C2_X_CFIRST 4 // control matched to C2_X_CMASK: constructed-mask claim, first exit call releases

// counters (index i lives at ctr[i * C2_CTR_STRIDE])
enum {
	CT_CLAIMS = 0,   // groups that got a slot
	CT_CLAIM_LANES,  // lanes in those groups
	CT_MISSES,       // groups that got no slot (empty pool or cap): counted, never waited for
	CT_MISS_LANES,
	CT_EXIT_LANES,   // lanes that ran the exit accounting with a slot
	CT_RELEASES,     // slots released
	CT_CAP_CLAIM,    // claim loop hit its atomic cap (the claim becomes a miss)
	CT_CAP_PUSH,     // stack push hit its cap (slot leaked)
	CT_CAP_SPIN,     // a lane's acquire wait for its claim hit the cap
	CT_TAG_BAD,      // owner tag at exit differed from the one written at entry
	CT_DBL_CLAIM,    // ledger: the claimed slot was still held by another claim
	CT_FOREIGN,      // ledger: a live lane found another claim's tag
	CT_FREED_LIVE,   // ledger: a live lane found its slot released
	CT_LANETAG_BAD,  // a live lane found its own tag word overwritten
	CT_BAD_REL,      // ledger: the releasing lane did not find its own tag
	CT_BAD_CLEAR,    // bitmap: the released bit was already clear (double release)
	CT_SPLIT_EXIT,   // claims whose first exit call did not hold all their lanes (checker)
	CT_TAGCTR,       // unique claim tags (checker)
	CT_CHECKS,       // checkpoint checks done
	CT_ID_CHANGE,    // hold: (%smid, %warpid) differed from the entry values (information)
	CT_RETRY,        // extra atomics in claims beyond the first
	CT_STEAL,        // claims served by a pool other than the first one tried
	CT_GAPS,         // hold: time steps > gap_ns seen by lane 0 of holding warps
	CT_GAP_MAX,      // hold: largest such step (ns)
	CT_HOLD_CAP,     // hold loop hit its iteration cap
	CT_BAR_CAP,      // full: co-residency barrier hit a cap
	CT_LAUNCHES,     // launches that ran (thread 0 of block 0)
	CT_SPIN_EXTRA,   // extra polls in the lanes' acquire wait (information)
	CT_HOLD_NS_MIN,  // hold: shortest hold (ns), atomicMin
	CT_MASK_CALLS,   // mask reproducer: helper calls
	CT_MASK_DISAGREE,// mask reproducer: the leader's __activemask() differed from the lane's
	CT_MASK_NOTIN,   // mask reproducer: the lane was not in its leader's mask
	CT_N
};

// The pool as the device sees it: one __constant__ copy per module (like bpftime's map_info).
struct PoolDesc {
	uint64_t bm;     // u64 bitmap words: npools x wpp
	uint64_t head;   // u64 stack heads: npools x C2_HEAD_STRIDE (version << 32 | index)
	uint64_t next;   // u32 next index per slot
	uint64_t hdr;    // u64 per-slot header, C2_HDR_WORDS per slot
	uint64_t tags;   // u64 per slot x 32 lanes (owner tags)
	uint64_t ctr;    // u64 counters
	uint32_t nslots; // pool size in slots (one slot = one claiming group of up to 32 lanes)
	uint32_t npools; // bitmap regions or stacks
	uint32_t spp;    // slots per pool
	uint32_t wpp;    // bitmap words per pool
	uint32_t wid_S;  // %warpid-addressed baseline: slot = (%smid % S) * W + %warpid % W
	uint32_t wid_W;
	uint32_t aba_ns; // stack pop: sleep between reading head/next and the CAS (ABA window)
	uint32_t lw;     // per-lane pools: bits per lane row in a region (warp slots); wpp = words per row
};

// Kernel arguments of the correctness kernels (by value).
struct XArgs {
	uint64_t in;       // const u32 *: random words
	uint64_t tl;       // u64 *: per-launch [2*id] first entry, [2*id+1] last exit (0 = off)
	uint64_t arrive;   // u32 *: full: arrival counter
	uint64_t hold_ns;  // hold: base hold
	uint64_t hold_step_ns; // hold: extra hold per lane group (lane & 3)
	uint64_t hold_jit_ns;  // hold: extra hold per warp, hash-spread over [0, hold_jit_ns]
	uint64_t bar_cap_ns;
	uint32_t nin;      // words in `in` (power of two)
	uint32_t off;      // per-launch offset into `in`
	uint32_t iters;
	uint32_t kase;
	uint32_t nthreads;
	uint32_t rounds;   // churn
	uint32_t J;        // churn: exit points per round
	uint32_t sleep_ns; // hold
	uint32_t gap_ns;   // hold
	uint32_t total_warps; // full
	uint32_t bar_cap_iters;
	uint32_t launch_id;
	uint32_t check_every; // hold: checkpoint every N polls
	uint32_t shape;       // mask reproducer: 0 leader loop + noinline, 1 one atomic + noinline,
	                      // 2 leader loop + inlined
};

// div kernel cases
#define K_EARLY 0
#define K_LOWLANES 1
#define K_NESTED 2
#define K_LOOPS 3
#define K_SPLIT 4
#define K_CHURN 5
#define K_ENTRYDIV 6 // lane-dependent sleep before the one entry call: groups enter the helper apart
#define K_CHURNSYNC 7 // churn with __syncwarp() before every claim: one full-warp group per claim

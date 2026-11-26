# Device hash map, round-one checks (rules written before any run, 2026-11-26)

Design: results gpu/hash-design-20261126/DESIGN.md (stage one: a device key
directory with permanent slots claimed by a 64-bit CAS, and 1531-style
(SM, lane) sharded rows for add-only u64 values). Nothing is built; these
checks decide what to build. Build for every GPU run: proto/builds/
tvf-7d02d677 (the current prototype; its lat_mmvq PTX is 81ea6349).

## C1, what llvmbpf emits (compile level; GPU runs only to dump PTX)
Programs: cuda-counter, cudagraph, threadscheduling, th-hostcall (built
here; PTX dumped from short runs on their own victims), gpu_shared_map,
threadscheduling_dynamic_hook, benchmark/gpu/micro (bytecode only: not
built here), pt_hash variants (PTX from pt_victim dumps).
Per map and per access through a lookup result: width; atomic or plain;
for atomics XADD (result unused) or FETCH (result used); in the PTX,
whether the access is generic or .global and whether plain u64 accesses
come out as two 32-bit accesses.
Rule: a map is stage-one material only if every access through its lookup
results is a 64-bit atomic add with an unused result (plus null tests).
Every other map is listed with the reason; nothing else follows from C1.

## C2, the hit path by hand on mul_mat_vec_q (tg128, mul_mat_vec_q probed)
Forms are edits of the current build's generated PTX (dumps of this
round), timed through the debug override, items shuffled, clocks checked
per item, two passes.
- lat_mmvq (runtime key: the log2 bucket), dump 81ea6349:
  - t0: the dump unchanged (today's 1531 inline lookup + red.global; the
    reference, "addmin" in the design);
  - hh: the sum lookup replaced by a directory hit at the key's home slot:
    hash (key ^ seed) x 0x9E3779B97F4A7C15 >> 54 over 1024 slots, one
    ld.relaxed.gpu.global.b64 of the header from a .global directory in the
    module (initialized in the PTX), key and state compared, the row taken
    from the header, the shard address formed as 1531's, red.global; a
    mismatch walks to the next slot (not taken in hh);
  - hh2: the same with every key placed at the slot after its home (the
    walk's second load taken);
  - hcall: hh's walk in a .func called from the probe.
  Rows equal the 1531 row of the key, so the counts stay checkable (sum =
  threads, bucket 194 = 0).
- count_mmvq (constant key, exit adds 1 to key k*65), its dump from this
  round:
  - c0: unchanged; chh: the row read from the header (ptxas may fold the
    home-slot address); chpre: the row fixed in the PTX, the header load
    only decides presence.
Rules (ns per thread over base, per pass):
- (a) hh - t0 <= 0.02 in both passes: build the inline path as designed;
  0.02 < hh - t0 <= 0.05: build it inline, and pre-bound constant keys if
  chpre - chh <= -0.006 in both passes; > 0.05: report that hash counters
  cost that much more than 1531 arrays.
- (b) inline only if hcall - hh >= 0.006 in both passes.
- (c) hh2 - hh is the price of one collision step; reported.
- A difference that moves more than 0.006 between passes gets a third pass.
- Counts exact in every item (lat_mmvq: sum and bucket 194; count_mmvq:
  sum = threads), the override logged.

## C3, SASS and registers (compile only)
ptxas -v and cuobjdump of every C2 form: registers and local frame of the
mul_mat_vec_q kernel and the probe subroutines' SASS. Plus one module-level
PTX snippet with every new form (atom.relaxed.gpu.global.cas.b64,
atom.acquire.gpu.global.cas.b64, ld.relaxed/acquire.gpu.global.b64,
st.release.gpu.global.b64, fence.acq_rel.gpu, nanosleep, red.global.add.u64)
compiled for sm_87 (the patched modules' target after rewrite_ptx_target).
Rules: the SASS shows the .global strong forms (LDG.E.64.STRONG.GPU and the
like, no generic forms); a form whose kernel gains registers or loses warps
per SM keeps the helper call; reported per form.

## C4, the claim protocol by hand (standalone CUDA test, no bpftime)
Helper bodies as inline PTX in a CUDA program. 10^6 threads x 10 launches;
each thread: lookup(key); hit: red.add 1 into its (SM, lane) shard; miss:
insert(key, 1, NOEXIST) with the bound reservation, CAS claim, row from
next_row, add, st.release publish; lost to the same key: bounded wait,
then EEXIST (counted). Keys: 64 distinct, heavily duplicated; max_entries
128 (and a run with max_entries 48 < 64 keys: E2BIG counted). Afterwards
every header is scanned.
Checks: no key in two slots; slots claimed <= max_entries; per key, the
delivered sum (all shards of its row) = attempts - EEXIST - E2BIG of that
key; unstable claims (bounded waits that ran out) reported.
Controls (each must fail, or the test is reported as unable to see that
fault): claim with a plain store instead of the CAS (keys in two slots
expected); claim without the bound reservation at max_entries 48 (more
than 48 slots expected). A publish without release is also run; if it
does not fail, that is reported as not observable by this test.
Rule: any violation with the protocol stops the build; zero violations
read as "consistent with the model", not as proof.

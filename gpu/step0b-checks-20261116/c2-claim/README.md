# Step 0b, check C2: can a warp claim a storage slot by itself?

Design note: `/home/jetson/src/results/gpu/dir1-design-space-20261115/README.md` (sections 2 and 3).
Step 0: `/home/jetson/src/results/gpu/step0-checks-20261115/README.md` (checks 1, 2 and 7 are reused here).
Standalone CUDA plus one module built by the dev tree's real passes. Nothing here edits the dev tree or `step0/`.

**Status: built and smoke-tested only. No measurement has been taken.** Smoke runs (`smoke/`) shared the GPU with
other agents and used scaled-down counts; they are development runs, not results. The real run is `run_all.sh` on a
quiet board, twice, serially.

## The question

From the task:

> Can a warp claim a storage slot by itself, with correctness that does not depend on what `%warpid` means, cheaply
> enough? At entry, one lane per warp (lowest active lane) pops a free slot index from a pool in device memory with
> atomics (one global pool; one pool per SM selected by `%smid`, a locality hint only; a bitmap with atomicOr/atomicAnd
> vs a lock-free stack with a version tag against ABA) and broadcasts it with `__shfl_sync`; every lane keeps the slot
> in a local; per-lane exit accounting: each lane decrements the slot's live-lane count when it exits and the lane
> that brings it to 0 releases the slot. Pool size = the number of resident warps; an empty pool is a counted miss,
> never a wait.

It decides:
1. **Correctness:** no two live claims ever hold the same slot, under full occupancy, divergent exits, two streams,
   CUDA graph replays and time-slicing from a second process, with the first-exit-release control failing.
2. **Cost:** ns per thread execution and per warp of claim + owner tag + per-lane exit accounting + release, against
   (i) a `%warpid`-addressed slot with the same owner tag (no claim) and (ii) the empty probe; [projection] over
   192.56M thread executions in 204.6 ms (one pp512).
3. Whether self-claim is cheap enough that correctness need not rest on `%warpid` semantics.

## What development found, and what was added because of it

**The task's mechanism, as written, cannot carry correctness: it fails on this GPU, and the CUDA guide's text for
it is ambiguous (below).** In the churn case (many claims per
warp, lanes leaving each claim at different steps) the `__activemask()` warp claim showed double ownership in every
pool. A reproducer (`x_mask_*`, no pool at all) isolates the cause:
- Pattern: `mask = __activemask()`; the lowest lane does some work; every lane reads its leader's mask with
  `__shfl_sync(mask, mask, leader)`.
- Smoke results:
  - With a loop in the leader-only region, 39,143 (noinline) and 116,089 (inlined) of about 4.46M calls had a lane that was
    not in its leader's mask (`smoke/run2`).
  - With a single atomic in that region: 0.
  - The driver-JIT path gave the same counts.
- So lanes reached one `__shfl_sync` with masks that disagree.
- **Scope:** the failure was seen where one warp's lanes enter the helper again and again in fragmented groups (churn:
  78-117 groups per warp for 16 claims; the reproducer has the same structure). Every case with one claim per warp
  (early, lowlanes, nested, loops, split, entrydiv, churnsync, full, streams, graph, hold) showed 0. That 0 is
  measured, not guaranteed: the documentation does not tie mask agreement to "one claim per warp".
- **What the CUDA guide says (v13.5, re-read 2026-11-15 by the review; the earlier version of this README quoted
  5.4.6.1 without its "unless" clause).** The text is not one-sided:
  - 5.4.6.1: `__activemask()` "is intended for opportunistic warp-level programming and only provides an
    instantaneous snapshot of the active threads within a warp", and "Note that threads convergent at an
    __activemask() call are not guaranteed to remain convergent at subsequent instructions unless those
    instructions are warp synchronizing intrinsics (__sync)."
  - Read plainly, that sentence and 5.4.6.6 ("Warp threads may call the intrinsic concurrently with different mask
    values, provided the masks are disjoint. Such condition is valid even in divergent control flow.") would allow
    the task's pattern: every lane of one snapshot calls `__shfl_sync` with that snapshot, and snapshots taken at
    different times by disjoint groups are disjoint.
  - The same section's own example, right after that sentence, makes the reading ambiguous: `mask =
    __activemask()`, then a non-sync line, then `__any_sync(mask, predicate)`, commented "Active threads might not
    be preserved", because "the compiler could reorder instructions, and the set of active threads might not be
    preserved". Whether that example falls under the "unless" clause is not stated.
  - 5.4.6.6 makes the result undefined if "A non-exited thread specified in the mask fails to either eventually exit
    or call the intrinsic at the same program point with the same mask value". The reproducer's counts are exactly
    that: lanes met at one `__shfl_sync` with masks that disagree.
- **Classification.** The guide's text is ambiguous for this pattern, and on this toolchain and GPU the pattern
  fails. Under this round's rule (correctness only from documented guarantees) it
  cannot carry correctness, whichever reading is right; a "not reproduced" on another GPU would not change that.
  Whether this is a toolchain deviation from the plain reading or behaviour the guide permits is not decided here.
- **Likely mechanism (a hypothesis from the SASS, not verified):** in the failing shapes the leader-only loop holds a
  `YIELD` inside a `BSSY`/`BSYNC` convergence region (B3 in the noinline helper, B1 and B3 in the two inlined copies), and `__shfl_sync` has a fast path (`R2UR` of the calling
  lane's mask, `BRA.DIV`, then `SHFL.IDX` without `WARPSYNC`); the single-atomic shape has no `YIELD` in that region.
  Several groups of one warp inside the helper at once (3.2.2.1.1: the GPU "can yield execution at a per-thread
  granularity") could then pass the shuffle with another group's leader. See `build/kernels.sass`, `x_mask_loop_ni`
  and `x_mask_one_ni`.
- Step 0 check 7's helper had the single-atomic shape. Its 0 violations therefore hold for that code shape, not in
  general. `step0/e7-warp` was not edited.

Two variants were added so the check can still answer the question:
- **Constructed mask (`oc_lane`, the documented form of the task's mechanism).**
  - The mask is the set of lanes that exist in the warp, derived from `blockDim`. The CUDA guide 3.2.2.2: "each warp
    contains threads of consecutive, increasing thread IDs with the first warp containing thread 0". 3.2.2.1 counts
    "the last threads of a block whose number of threads is not a multiple of the warp size" as inactive, so their
    bits are 0.
  - It is valid only where every thread of the warp calls the helper at the same program point: a kernel's first
    statement, or after a full `__syncwarp()`.
  - It is therefore run only in cases that claim that way. In bpftime this means a claim done by the runtime at
    kernel entry, not by a tool's conditional code.
- **Per-lane claims (`ln*`, `lf*`), outside the task's stated mechanism, with their own rules.**
  - Every thread claims its own slot, so no warp collective is involved at all.
  - `ln*`: one `atom.or` on a lane-major bitmap, so a warp's lanes hit 32 different words; released with one `atom.and`.
  - `lf*`: one 32-bit flag word per thread slot; claimed with `atom.cas 0->1` (acquire), released with `st.release 0`.
  - Memory equals the warp pools' memory: 12,288 thread slots = 384 warp slots x 32 lanes.

The decision rules below were written after the smokes that showed this failure, and before any real run. They do
not depend on smoke timings.

## Mechanisms under test

Source: `src/c2.cuh` (helpers, `__noinline__` like bpftime helpers), `src/kernels.cu` (kernels).
Pool memory is device memory (`cuMemAlloc`). The pool descriptor is the module's `__constant__ g_pd`, as bpftime's
`map_info` is.

**Pools.** Slots = factor x resident warps.
- Resident warps come from documented driver attributes: `MAX_THREADS_PER_MULTIPROCESSOR / 32 x MULTIPROCESSOR_COUNT`
  = 48 x 8 = 384. `%nwarpid x %nsmid` gives the same 384 here, but `%nwarpid` is a ptxas constant (step 0, check 1).
- Per-lane pools use factor x 12,288 thread slots.

| pool | claim (leader for warp pools, every lane for lane pools) | release | id hint (performance only) |
|---|---|---|---|
| `bmsm` | `atom.or.b64` on the SM's bitmap region; on a taken bit, retry with the free bits the atomic returned, then other regions | `atom.and.b64` (old value checked: double release counted) | region `%smid % 8`, first bit `%warpid` |
| `bmglob` | the same on one region | the same | first bit from a hash of the logical warp index |
| `stksm` | Treiber stack, head = version << 32 \| index; pop = load head, load next, `atom.cas` | push = `atom.cas` loop | stack `%smid % 8`, others when empty |
| `stkglob` | one stack | the same | none |
| `stksmnv`, `stkglobnv` | **control:** the same stacks without the version tag | | |
| `lnsm`, `lnglob` | per lane: `atom.or.b64` in its lane row (32 rows of W bits per region) | `atom.and.b64` | (`%smid`, `%warpid`, `%laneid`) / (hash, `%laneid`) |
| `lfsm`, `lfglob` | per lane: `atom.cas.b32 0->1` on the slot's flag word | `st.u32 0` | the same |

**Warp claim, entry (`c2_entry`).**
1. `mask` = `__activemask()` (`o_lane`, `r_lane`, `o_agg`, `o_first`) or the constructed mask (`oc_lane`, `oc_first`).
2. The leader is the lowest lane of the mask, and `n = popc(mask)`.
3. The leader pops `s` and then does `old = atom.add(live[s], 1 << 32 | n)`. The claim's generation is
   `gen = old.hi + 1`.
4. `s` and `gen` are broadcast with `__shfl_sync(mask, ., leader)`.
5. Each lane waits, capped at 512 polls, until `ld.acquire(live[s]).hi == gen`.
6. Each lane writes its owner tag `tags[s * 32 + lane]` and keeps `(s, gen, tag)` in registers.
7. On a miss (empty pool, or 1,024 atomics without success), every lane gets no slot and the miss is counted.

**Warp claim, exit (`c2_exit`, per lane).**
1. The lane reads its owner tag back; a mismatch is counted.
2. `lane`/`oc_lane`: `old = atom.add(live[s], -1)`. The lane with `(u32)old == 1` releases the slot.
3. `agg` (not a decision variant): `__match_any_sync`, `__syncwarp(peers)`, then one decrement by the exit group's
   leader.
4. `first` (**control**): the first exit call of a claim releases the slot. `o_first` claims with `__activemask()`,
   `oc_first` (added by the review) with the constructed mask, so each warp-claim group has a control with its
   own claim.

**Lane claim.** Every lane claims, writes its tag and releases its own slot. There is no count. The control `o_first`
releases the slot right after the claim and keeps using it.

**Ordering (`o*` vs `r_*`).**
- `o` uses only PTX memory-model orderings, at `.gpu` scope:
  - acquire on the claim RMW (or on the stack's head load);
  - release on the leader's add;
  - every lane's `ld.acquire` wait;
  - `acq_rel` on each lane's decrement;
  - release on the bitmap clear or stack push (lf: `st.release`).
- `r` uses `.relaxed` everywhere. It is reported for the price of ordering and is never a decision row.

**Baselines.**
- `c_wid`: slot = (`%smid` % 8) x 48 + `%warpid` % 48, with the same owner-tag store (`st.relaxed.gpu`) and check
  (`ld.relaxed.gpu`) as the claims.
- `c_probe`: the same two `__noinline__` calls with empty bodies.
- `rp_*`: today's empty probe pair, real pass output. `ref/build_refprobe.py` drives the dev build's
  `libptxpass_kprobe_entry.so` and `libptxpass_kretprobe.so` through `process_input()` with the eBPF program
  `r0 = 0; exit`, wrapped as `hack_fatbin` does (adapted from `step0/e2-cost/ref/build_ref.py`).
- `rp_*` compiles to two `CALL`s of functions that only return. ptxas drops the probe switch's load because both of
  its outcomes return.

## The documented guarantees each part relies on

PTX ISA 9.5 (docs.nvidia.com, read 2026-11-15) and the CUDA Programming Guide (online v13.5, read 2026-11-15; the board
runs CUDA 13.2).

| what | guarantee |
|---|---|
| slot-index exclusivity (every pool) | PTX 8.10.3, atomicity of RMW operations: "When an atomic operation A and a write W overlap and are morally strong", A cannot both read from a write that precedes W in coherence order and follow W in coherence order. Bitmap and stack words are written only by atomics of one size at `.gpu` scope; an `lf` flag word is written by the claim's `atom.cas` and the release's `st.release`/`st.relaxed`, all strong 32-bit `.gpu` operations, so morally strong (8.7). Two claims of one free value cannot both succeed. The scans' plain strong loads are hints only. Exclusivity needs no ordering qualifier. |
| last-lane detection | the same axiom on `live[s]`: exactly one decrement observes count 1 |
| hand-off of slot data between owners (`o` only) | PTX 8.8 release/acquire patterns ("A release operation on M ... st.release [M]; or atom.release [M]"; "An acquire operation on M ... ld.acquire [M]; or atom.acquire [M]"); 8.9.2 observation order, "through an optional sequence of atomic read-modify-write operations" (other slots' set/clear on a shared bitmap word sit in between); 8.9.4 "A release pattern X synchronizes with an acquire pattern Y, if a write operation in X precedes a read operation in Y in observation order, and the first operation in X and the last operation in Y are morally strong" (all of them are strong `.gpu` operations on global memory, so morally strong by 8.7); base causality order (8.9.5) is transitive, which chains owner -> last lane -> next leader -> its lanes |
| non-leader lanes ordered after the previous owner (warp claims, `o`) | each lane's own `ld.acquire` reads the leader's release add (or a later RMW), so no warp-level ordering is used. 8.5: "the warp is not a scope"; `bar.warp.sync` is not among 8.9.4's synchronizing operations (`step0/e4-publish` README) |
| `atom` without qualifiers | PTX 9.7.15.5: ".relaxed is assumed by default", ".gpu scope is assumed by default" (counters only) |
| `__shfl_sync` with a constructed mask | CUDA guide 5.4.6.6 constraints (each calling thread in the mask, non-calling threads 0, the same mask value at the same program point) + 3.2.2.2 warp partition + 3.2.2.1 inactive tail lanes |
| `__shfl_sync` with `__activemask()` | **not usable for correctness** (see above): 5.4.6.1's "unless those instructions are warp synchronizing intrinsics" is made ambiguous by its own example ("Active threads might not be preserved"), and disagreeing masks, which 5.4.6.6 makes undefined, were measured here |
| `agg` exit (informational) | `bar.warp.sync`'s instruction-level text only, which is outside the formal model (`step0/e4-publish`) |
| `%smid`, `%warpid`, `%laneid` | hints only; a wrong value costs extra atomics or a miss, never exclusivity |

## Method

### Correctness: `build/c2chk`, driver API

- **Checker** (kernels `x_*`, CHK = 1). Each claim takes a unique tag from a counter.
  - The leader `atom.exch`es it into the slot's ledger word, which must have been 0 (`dbl_claim`). It does so
    before its release add on `live[s]`, so each lane's `ld.acquire` of that add also orders the ledger value for
    the lane (`sass_check.py` checks the order in the PTX). Before the review the exch came after the add, so a lane
    could, by the PTX model, read the previous 0 and count a `freed_live` or `bad_rel` without any fault of the
    mechanism (none was seen).
  - Every lane keeps `tag << 12 | n << 6 | lane` in its own tag word.
  - At every checkpoint and at exit, every live lane reads the ledger and its tag word with strong loads.
    - Ledger 0 counts as `freed_live`.
    - Another claim's tag in the ledger counts as `foreign`.
    - A changed tag word counts as `lanetag_bad`.
  - The releasing lane `atom.cas`es the ledger from its tag to 0 (`bad_rel`).
  - The bitmap clear checks that the bit was set (`bad_clear`).
  - `violations` is the sum of these counts plus `tag_bad`.
  - Accounting: exit lanes = claimed lanes, releases = claims, no push/spin cap hit, and after every run every slot is
    free, every live count 0 and every ledger 0 (host check of the bitmaps, the stack walk and the headers).
- **Split exits**, the opportunity count, uses no warp collective. The first decrementer of a claim stores the exit's
  step id. The releasing lane compares its own step id with it and counts `split_exit` when they differ.
- **Cases.**
  - `div`: the step0/e7-warp patterns, grid = 4 x resident blocks of 128, pool factor 0.5 and 1. Each run lasts
    until max(100,000 claims, 100 x slots).
    - `early`: lanes leave at random iterations.
    - `lowlanes`: lane 0 and the lowest 1-31 lanes leave first.
    - `nested`: exit sites in nested branches.
    - `loops`: checks inside lane-dependent loops.
    - `split`: the claim runs under divergence, up to 3 groups per warp, `__syncwarp()` between them.
    - `entrydiv`: a lane-dependent sleep before the one claim, no barrier. The smokes showed exactly one claim group
      per warp (the compiler reconverges before the call), so this adds a pre-claim sleep, not a split entry; the
      summary prints groups per warp for every case.
    - `churn`: 16 claims per thread, lanes leave each claim at different steps.
    - `churnsync`: churn with `__syncwarp()` before each claim.
  - `aba`: churnsync with 64 rounds, one step per round and a 5 us sleep between reading head/next and the CAS. It
    runs the four stacks with constructed-mask claims, so the `__activemask()` effect cannot mix in, until 1M claims.
  - `full`: a capped all-warp barrier after the claim (step0/e1-ids case a) proves that all warps held their slots at
    once.
    - Block sizes 32, 48 (partial warps), 64, 96, 128, 256, 512 and 1,024, three repetitions each, pool factor 1.
    - Lanes leave in four groups.
    - Control: one launch of twice the resident capacity must hit the barrier cap.
  - `streams`: two non-blocking streams, 30 launches each of 2 x resident blocks, sharing one pool. Each launch's first
    entry and last exit are logged, and overlapping launch pairs across the streams are counted.
  - `graph`: a 3-kernel graph captured from a stream and replayed 200 times; launches are counted on the device.
  - `hold`: two waves of 96 x 128 threads hold their slots 8-22 ms (base 8 ms, +2 ms per lane group, +0-8 ms per
    warp).
    - Checks every 16 polls with a 2 us sleep; lane 0 counts time steps > 200 us.
    - Run once alone (`hold_ctl`) and once while `step0/e1-ids/hog.cu` (copied to `src/hog.cu`) keeps a second
      context busy with full-occupancy 50 ms kernels (`hold_hog`).
  - `mask`: the reproducer, 3 shapes, churn structure.
- **Paths.** `native` runs the nvcc cubin. `jit` runs `build/kernels_jit.ptx`, a subset whose `call.uni` instructions
  are rewritten to plain `call` as bpftime inserts them, loaded with `cuModuleLoadData`. The subset is mask, div with
  `bmsm`/`stkglob`/`lnsm`/`lfsm`, and the controls.

### Cost: `build/c2bench`, with step0/e2-cost's bench2 method

- Kept from bench2:
  - the `k_hold` launch gate;
  - the shapes anchor 512 x 512, large 8,192 x 512 and full 24 x 512;
  - one validation launch per item;
  - N calibrated to about 4 ms per batch;
  - 10 rounds, each in an order shuffled by `mt19937_64(seed + round)`;
  - 1 untimed launch, then N launches between events;
  - delta against the empty kernel of the same module, shape and round.
- Added:
  - Before every batch the item's pool is re-initialised, outside the timed region.
  - Misses, cap hits and tag mismatches are read before and after each batch.
  - The pool's end state is checked after each batch.
  - A 2x pool row for `bmsm_o_lane`, `bmsm_oc_lane`, `stkglob_o_lane`, `lnsm_o_lane` and `lfsm_o_lane` shows misses
    from releases still in flight.
- Kernels (`c_*`): one entry helper call and one exit helper call per thread, signature `(u64 *aux, u32 mode)`.
- Validation: claimed + missed lanes = threads, exit lanes = claimed lanes, releases = claims, tag mismatches 0, end
  state clean.

### SASS and PTX checks: `sass_check.py`, run by `build.sh`

- **PTX, per helper instantiation:**
  - the `.acquire`/`.release`/`.acq_rel` qualifiers where ordered, and none in relaxed builds;
  - `activemask.b32` except in constructed-mask builds;
  - `shfl.sync.idx`;
  - in `agg`, `match.any.sync` and `bar.warp.sync`.
- **SASS, per kernel, for native and for the offline-assembled JIT PTX:**
  - the helpers stay real `CALL.REL.NOINC` targets;
  - `MEMBAR.ALL.GPU` and `CCTL.IVALL` in ordered builds, neither in relaxed ones;
  - the expected atomics;
  - the warp claims' per-lane decrement is an `ATOMG.E.ADD.64` without the compiler's warp-aggregation election
    (one atomic per lane, as it would be in bpftime, where the slot reaches the exit helper as an argument);
  - owner-tag loads are `LDG.E.64.STRONG.GPU`;
  - `c_wid` reads `SR_VIRTUALSMID` and `SR_VIRTID`;
  - the inlined reproducer has no helper call.
- In a single-call-site kernel, ptxas compiles `__activemask()` as `VOTEU.ANY` into a uniform register; that is
  accepted.

## Decision rules (fixed before any real run; constants at the top of `analyze_chk.py` and `analyze_cost.py`)

### Correctness, per decision variant

The decision variants are `o_lane` (task, `__activemask()`), `oc_lane` (task, constructed mask) and the lane pools'
`o_lane`, each with every pool.

- **incorrect** if any of these holds in any run, native or JIT, including `full`, `streams`, `graph`, `hold_hog`,
  `hold_ctl` and, for the constructed-mask group's stacks, `aba` (it runs constructed-mask claims only):
  - violations > 0;
  - exit lanes != claimed lanes or releases != claims;
  - a push or spin cap hit;
  - an end state that is not clean;
  - an empty-pool miss in `full`, where the pool equals the resident count. Cap misses there (the claim's
    1,024-atomic cap, CAS contention) are reported as contention, not as incorrect.
- **correct** if none of those holds, and every one of these:
  - every native `div` case (oc: not split, entrydiv, churn), at both factors, reached 90% of its claim target and
    >= 100 claims per slot;
  - for warp claims, >= 10,000 split exits in early, lowlanes, nested, loops, entrydiv and churnsync;
  - every `full` launch passed its barrier (0 cap hits) and had 48 warps per SM at block sizes 96-512;
  - the oversubscribed `full` launch hit its cap;
  - `streams` had >= 10 overlapping launch pairs and every launch counted;
  - `graph` counted 3 x 200 launches;
  - the claim atomic was arbitrated: retries + claims served away from the hinted slot >= 1,000, summed over the native
    `div` runs at factor 0.5. At factor 1 the `%warpid`-hinted pools may never collide (in the smokes they did not),
    which would leave the atomic untested;
  - `hold_hog` had >= 100 warp-gaps > 200 us, a largest gap >= 1 ms, a shortest hold >= 1 ms and no iteration cap
    hit, with the hog still alive at the end (time-slicing shown);
  - **the group's control of the same pool showed double ownership, `dbl_claim + foreign + lanetag_bad > 0`, in
    every one of early, lowlanes, nested, loops, entrydiv and churnsync at factor 0.5.** `freed_live` alone does not
    count: the control's own release clears the ledger even if nobody re-claims the slot. The controls: `o_first`
    for the `__activemask()` group, `oc_first` for the constructed-mask group (its cases, so without entrydiv), and
    for lane pools `o_first`, which releases right after the claim.
- **inconclusive** otherwise, with the reasons listed.

The group verdict is "correct (every pool)", "correct for <pools>", "incorrect" (no pool correct, at least one
incorrect) or "inconclusive".

Not judged:
- `r_lane` and `o_agg` are reported (violations, failing cases).
- `hold_ctl`'s gap counts are reported; on a quiet board 0 is expected but not required.

### ABA control (reported, does not decide the stacks)

- "provoked" if both no-version stacks show violations > 0 while both versioned stacks show 0.
- "not provoked" if either no-version stack shows 0 while the versioned stacks show 0; the version tag's necessity
  is then not shown. Any violation in a versioned stack's ABA run also makes that stack's correctness verdict
  "incorrect".

### Reproducer

- "reproduced, tied to a loop in the leader-only region" if `loop_ni` > 0 and `loop_inl` > 0 while `one_ni` = 0.
- "reproduced in every noinline shape" if `one_ni` > 0 as well.
- "not reproduced" if all three are 0.
- Native and JIT are reported separately.

### Cost, decision at the anchor shape

- **Increment.** I = median over rounds of the paired t(variant) - t(`c_wid`), per thread execution.
- **Variant used.** In each group, the cheapest `o` variant with per-lane accounting whose correctness verdict is
  "correct" (if none, all variants, marked as such).
- **Classes.** "small" if I <= 0.053 ns (5% of one pp512 [projection]); "moderate" if I <= 0.106 ns (10%); "large"
  otherwise.
- **Rows that cannot carry the decision** (added by the review): a missed claim skips the claim's work, so a row
  with misses looks cheaper than the mechanism is.
  - A row is excluded if its validation failed, if any timed batch left the pool unclean, or if it counted
    `cap_spin` or `tag_bad`.
  - If its miss rate (missed lanes / lanes) exceeds 1%, its 2x pool row is used when that row has none of these
    problems; otherwise the variant is excluded.
  - Exclusions are listed with the decision.
- **Self-claim is cheap enough that correctness need not rest on `%warpid`** if a group that is correct is "small"; a
  correct group that is "moderate" is reported as a trade-off.
- **Reported, not decided:**
  - ns per thread execution and per warp, with [min-max];
  - [projection] = ns x 192.56M / 204.6 ms;
  - the increment over `c_probe`;
  - `o` vs `r` (the price of the documented hand-off);
  - the real empty probe pair `rp_pair` against `rp_empty`;
  - today's 1512 op (0.433 ns, step 0 check 2) as reference;
  - large and full as secondary shapes.
- A delta is "resolved" only if it is positive in every round.

## Commands (main session, quiet board, clocks checked)

```
cd /home/jetson/src/bpftime-gpu-work/step0b/c2-claim
./run_all.sh                 # -> runs/<date-time>/ ; or ./run_all.sh <OUT>
```

- **Timeouts.** The total timeout is `TOTAL_S` (default 2,400 s): the script re-executes itself under
  `timeout -k 10`. Each step runs in the background under its own `timeout -k 5` and is waited for; when the total
  timeout fires, a trap sends TERM to the running step's `timeout` and to the hog (each `timeout` sits in its own
  process group, so the outer signal alone would not reach them). The two analysis scripts run under
  `timeout -k 5 120`. Per step:
  - build 600 s;
  - native 1,500 s;
  - jit 400 s;
  - hold_ctl and hold_hog 150 s each;
  - the hog `HOG_S + 25` s;
  - bench 900 s.
- **Steps.**
  1. `build.sh`, which runs `sass_check.py`; the script stops unless it prints `RESULT PASS`.
  2. `analyze_chk.py --selftest`.
  3. `c2bench` (10 rounds), first, as step 0's second pass ran its timed steps first.
  4. The native correctness run.
  5. The JIT correctness run.
  6. `hold_ctl`.
  7. The hog (30 s) and `hold_hog`.
  8. `analyze_chk.py` and `analyze_cost.py`.
  It logs the clocks, GPU load and busiest processes before and after, and `/dev/shm` before/after.
- **Shared memory.** Nothing creates POSIX shm (the gate uses `cuMemHostAlloc`). CUDA's own
  `/dev/shm/cuda.shm.<uid>.<pid>.*` leftovers are removed only for our pids.
- **Expected wall time:** about 7-10 minutes.
  - Build about 1 minute (56 s in the smoke).
  - Native correctness about 2-5 minutes: about 740 runs (the review added 40 control runs). The smokes' 9-11 s for 696-736 rows used scaled-down claim targets.
  - JIT about 30 s, with 2.1 s of driver JIT at module load.
  - Holds about 45 s: the script waits for the 30 s hog to end.
  - Bench about 1-2 minutes: 120 items. The global-stack items at the large shape take about 150 ms per launch.
- **Smoke mode.** `SMOKE=1 TOTAL_S=420 ./run_all.sh smoke/<dir>` scales every step down; every GPU step stays under
  30 s, and the whole smoke run took 81 s.
- **Outputs:**
  - `OUT/chk_summary.{md,json}` (verdicts);
  - `OUT/bench/cost_summary.{md,json}`;
  - `OUT/{native,jit,hold_ctl,hold_hog}/results.jsonl` (one line per run, every counter, the end state);
  - `OUT/bench/raw.csv` and `items.json`;
  - `OUT/sass_check.txt`;
  - `OUT/run_all.log`.
- **Single runs:** `./build/c2chk --help`-style options are listed at the top of `src/c2chk.cpp`
  (`--cases --pools --exits --factors --kases --min-claims ...`). `./build/c2bench --only REGEX --shapes ...` is also
  available.

## Changes made by the adversarial review (2026-11-15, before any real run)

- README: 5.4.6.1 was quoted without its "unless those instructions are warp synchronizing intrinsics (__sync)"
  clause, which reads in favour of the task's pattern. The section now quotes it in full together with the guide's
  own example, calls the text ambiguous, and classifies the pattern as "not usable for correctness" rather than
  "not covered". The
  8.9.4 quote now includes its moral-strength condition. A SASS-based hypothesis for the mechanism is labelled as
  such.
- Checker: the leader's ledger exch moved before its release add (see Method); `sass_check.py` checks the order.
- Matched control `oc_first` (constructed-mask claim, first-exit release) for the constructed-mask group, native in
  every warp pool and on the JIT path for `bmsm`. Before, that group was judged against `o_first`, whose claim
  uses `__activemask()`.
- Cost decision: rows with failed validation, unclean pools, `cap_spin` or `tag_bad`, or a miss rate above 1%
  without a clean 2x row are excluded. In the smoke, `stkglob_o_lane` (1.6% cap misses at both pool sizes) and
  `stkglob_oc_lane` (1.4%, no 2x row) would otherwise have been eligible with an understated cost.
- `run_all.sh`: the total timeout now stops the running step and the hog (before, it would have ended only the
  script, and a running step would have continued until its own timeout, up to 1,500 s; inferred from `timeout`'s
  process groups, not tested). On interruption it also removes CUDA's `/dev/shm` leftovers of our pids;
  `c2bench` now records its pid at start for that.
- Smoke after the changes: see "Development smokes".

## Known limits

- **Split-exit counter (power floor only).** The first decrementer stores its step id after its decrement, so the
  last lane's read of it is not ordered by the PTX model; the counter can be off in either direction. It feeds only
  the >= 10,000 floor, never a violation count; the controls are the real power check.
- **Documentation versions.** The guide quotes are from the online CUDA Programming Guide v13.5 (the board runs CUDA
  13.2); the PTX quotes from PTX ISA 9.5.
- **Interruption paths.** The total-timeout trap was tested during the build and native steps, not during
  `hold_hog` (the hog is signalled the same way).

- **One GPU, one driver.** The reproducer's rate depends on code shape and scheduling. A "not reproduced" elsewhere
  would not make `__activemask()` claims safe; the decision rests on the documentation, not on the rate.
- **The constructed mask is entry-only.** It requires every thread of the warp to call the helper at the same program
  point. A tool calling the helper conditionally would violate 5.4.6.6 and could hang. Cases that cannot satisfy this
  are not run with it.
- **Per-lane pools are outside the task's stated mechanism.** A warp's slots are contiguous only when the `%warpid`
  hint holds; otherwise storage access is uncoalesced, which affects performance only.
- **Pool size.** It comes from device attributes. In flight, a released slot may not yet be visible to the next warp
  on the same hardware slot; that shows up as misses, measured with the 2x row.
- **Cost on empty kernels.** The cost is fully exposed: no overlap with real work.
- **Time-slicing only.** Preemption is exercised only by a second process. Debugger, checkpoint and same-context
  priority preemption are not exercised.
- **Graphs.** Only a captured linear 3-node graph is tested; `cudaGraphExecUpdate` is not.
- **Device memory only.** Records the CPU reads and their publication are other checks' subjects.
- **Ordered-variant spin.** The cap of 512 polls was sized from the smokes: a lane saw its generation at once or never.
  A cap hit counts against correctness.
- **Global stacks.** The 1,024-atomic claim cap turns CAS storms on `stkglob` into counted misses (cap misses are
  reported apart from empty-pool misses). Their cost reflects that.
- **Reference module.** `rp_*` is compiled by offline ptxas, not by the agent's nvPTXCompiler. That is the same
  compiler version, as in step 0 check 2.

## Development smokes (not results; shared GPU, scaled-down counts)

Kept: `smoke/run3/` (the review's `SMOKE=1 ./run_all.sh` of the final code), `smoke/run2/` (the writer's last full
smoke, before the review's changes; its `bench/cost_summary.*` was regenerated by the review's `analyze_cost.py`) and
`smoke/aba2/` (the ABA case at its real 1M-claim target). Earlier development runs were removed because they predate
the final code.

Review smoke (`smoke/run3`, rc 0, about 80 s; every GPU step under 12 s; `/dev/shm` unchanged; nothing left
running): the same picture as run2 below. `__activemask()` claims fail in churn in every pool; constructed-mask and
lane pools 0 violations, inconclusive only because of the scaled-down floors; every `o_first` and `oc_first` control
run failed (each with at least 0.8M violations); reproducer native 37,926 / 0 / 116,109 (JIT 38,051 / 0 / 116,058).
ABA at the smoke's one-launch target was not provoked in the final run3 (0 and 0; two earlier attempts 0 and 0, 0 and
4.9M), while a separate run of this code at the real 1M target (11 launches) gave 15.3M (`stksmnv`) and 148.5M
(`stkglobnv`) violations with 0 in the versioned stacks (the writer's `aba2`: 15.4M and 150M). Tests of the total timeout: `TOTAL_S=8` stopped the build step, `TOTAL_S=63`
stopped `c2chk` in the native step; both rc 124, no process left, `/dev/shm` unchanged.

- **Reproducer** (run2): native `loop_ni` 39,143, `one_ni` 0, `loop_inl` 116,089 lanes not in their leader's mask, of
  about 4.46M calls each; JIT 38,890 / 0 / 116,175.
- **Churn with `__activemask()` claims:** violations in every warp pool, native and JIT, with ordered and relaxed
  builds and with the agg exit; no other case failed.
  - Example, `bmsm o_lane`, factor 0.5: `foreign` 22,379, `lanetag_bad` 3,344, `dbl_claim` 115, `cap_spin` 3,918.
  - Exit lanes (112,197) exceeded claimed lanes (108,950): lanes used a claim they were not counted in.
  - Claim groups per warp: 1 in early, lowlanes, nested, loops and entrydiv; 3 in split; 16 in churnsync; 78-117 in
    churn.
- **Constructed-mask warp claims and all per-lane pools:** 0 violations in every case and path. Their smoke verdicts
  are "inconclusive" only because the smoke's claim targets are below the floors.
- **Controls:**
  - First-exit release and early release: double ownership in every run, every pool.
  - ABA (aba2, 1M claims): `stksmnv` 15.4M and `stkglobnv` 150M violations, versioned stacks 0 (run2 at its
    scaled-down target: 3.3M and 11.7M, versioned 0).
  - The oversubscribed `full` launch hit the barrier cap (384 warps).
- **Arbitration:** at factor 0.5 every decision variant retried or claimed away from its hint (thousands to millions
  of times). At factor 1, `bmsm oc_lane`, `lnsm` and `lfsm` never collided: every claimer got its `%warpid` slot.
- **Holds next to the hog:** 750-766 warp-gaps per run, largest gap 2.51-2.53 ms, 0 `%smid`/`%warpid` changes, 0
  violations. Without the hog: 0 gaps.
- No timing from the smokes is used anywhere.

## Files

| file | what |
|---|---|
| `src/c2_shared.h` | pool descriptor, kernel arguments, counters (host and device) |
| `src/c2.cuh` | the mechanism: pools, `c2_entry`, `c2_exit`, checker, baselines |
| `src/kernels.cu` | cost kernels `c_*`, correctness kernels `x_div/full/hold_*`, reproducer `x_mask_*`, gate `k_hold` |
| `src/c2host.h` | host pool setup and end-state checks |
| `src/c2chk.cpp` | correctness runner |
| `src/c2bench.cpp` | cost bench (bench2's method) |
| `src/hog.cu` | copy of `step0/e1-ids/hog.cu` |
| `ref/build_refprobe.py` | today's empty probe pair from the dev passes |
| `sass_check.py` | PTX and SASS checks |
| `analyze_chk.py` | correctness verdicts (`--selftest`) |
| `analyze_cost.py` | cost tables and the cost decision |
| `build.sh`, `run_all.sh` | build and run |
| `smoke/` | development runs; not results |

# C3 (step 0b): cost parts of today's per-thread map op, slot addressing, fence kinds, thread-end costs

Status: built and smoke-tested only; reviewed (the review's changes are marked "added in review" or listed under "Review changes" at the end). **No timed measurement has been taken for (a), (b) and (d).** The smokes ran 1-2 rounds while other agents shared the GPU; none of their numbers is used anywhere. Part (c) needs no run: it is computed from step 0's real-run data and its output is in `threadend/out/`.

Design note: `/home/jetson/src/results/gpu/dir1-design-space-20261115/README.md`. Step 0: `/home/jetson/src/results/gpu/step0-checks-20261115/README.md` (check 2 = `e2-cost`, whose harness this check copies).

## Questions

- **(a) Middle term.** At anchor in device memory, step 0 measured an 8-B store at 0.032 ns per thread execution, the store plus `membar.sys` at 0.259 ns, and today's 1512 lookup-and-increment at 0.369 ns (0.433 ns registered). Where does the 1512 cost go: the probe call, the helper call and its map-type dispatch, the fence, or the memory operation? Is the raw-store band (3-6% projected) reachable only with an inlined helper path? Does the fence-free helper path still project 10% or more?
- **(b) Slot addressing.** What does a thread's hardware slot cost in device memory: the slot address computed from `%smid`/`%warpid`/`%laneid`, a tag store at entry and a tag check at exit? Separately, what does a claim-based form cost, built from documented primitives (acquire CAS, release store) instead of the measured exclusivity of hardware slots?
- **(c) Thread-end operations, from step 0's data.** What do merging and writing out at thread end cost, per thread execution and per warp record? The operations are: a per-warp `red`/`atom`, a per-lane atom, a per-SM word, and a ring record.
- **(d) Fence scope.** What does `membar.gl` (`__threadfence`) cost per warp against `membar.sys` (`__threadfence_system`), after a store and alone, in registered and device memory, at the three shapes? `fence.acq_rel.{gpu,sys}` are added as a secondary reading.

## Method

### Harness

`build/bench3` is a copy of step 0's `bench2` (`diffs/bench2_to_bench3.diff`). It keeps these parts of bench2 unchanged:
- the memory: 128 MiB registered as bpftime registers its segment, and 128 MiB of `cuMemAlloc`;
- `cuLaunchKernel` only;
- the `k_hold` launch gate;
- calibration to about 4 ms per batch;
- 10 rounds, each in an order shuffled by `mt19937_64(20261115 + round)`;
- CUDA events around N back-to-back launches.

The shapes are full = 24 x 512, anchor = 512 x 512 and large = 8,192 x 512. Rbig is 16, 8 and 2 for those shapes. Registered memory uses the shm name `step0b_c3_<pid>`, which is unlinked as soon as it is mapped.

### Modules

Each module has its own empty kernel, and every delta is taken against the empty kernel of the same module and shape.

| module | built from | contents |
|---|---|---|
| k | `src/kernels.cu`: step 0's kernels, unchanged, plus the C3 cases at the end (`diffs/kernels.cu.diff`) | (b), (d), and step 0's `st_lane32` / `st_lane1` f0/f1 kernels |
| F | `ref/build_variants.py`, exactly as step 0's `build_ref.py`: the dev build's `libptxpass_kretprobe.so` through `process_input()`, wrapped with the checked-in `trampoline_ptx.h` and the probe switch, `ptxas -arch=sm_87` | today's op. **Its PTX is byte-identical to step 0's `ref1512.ptx`**, and the inputs' sha256 match step 0's (`build/ref/variants.meta.json`) |
| N | the same pass output, wrapped with a COPY of the trampoline text minus the 1512 branch's `membar.sys` | fence-free helper path |
| X | the original trampoline plus the variant kernels below | E, I, IF, IC, K, M, M64 |

How the N copy is made:
- The script finds `setp.eq ... 1512; @p bra L` in `_bpf_helper_ext_0001` and walks the helper's control flow from L.
- It asserts that exactly one fence is reachable from there: `$L__BB2_36`, which the hit path and the bounds-failure path both pass.
- It removes that `membar.sys` with its two inline-asm comment lines.
- The diff against F must be exactly those 3 removed lines (`build/ref/trampoline_nofence.diff`).

### (a) The ladder

h = registered (fd 5) or device (fd 6), each at R = 1 (exit form) and R = Rbig (loop of probe calls). Every level validates that each thread's word equals R.

| level | what runs per thread |
|---|---|
| F | today: exit probe CALL -> probe switch load -> eBPF stack key -> CALL `_bpf_helper_ext_0001` -> map-type dispatch -> 3-D thread id, bounds -> `membar.sys` -> 2 x LD.E + 2 x ST.E (the JIT's u64 split) |
| N | F without the fence |
| I | N with the helper CALL replaced in the probe function by the 1512 fast path, written from the helper's PTX: map-type guard, key loaded from the eBPF stack, 3-D thread id, both bounds checks, address. A failed guard or bound is counted (`__c3_inline_miss`) and returns NULL. |
| IF | I plus the `membar.sys` where the helper has it |
| IC | I whose failure path is counted and then calls the real helper. Every map type, and a failed 1512 bound, behaves as with the real helper (which is linked in), except that IC's 1512 fast path has no fence, like I; so it is deployable only if that fence may be dropped, which C3 does not decide. |
| K | I's probe body pasted into the kernel, so there is no probe CALL. The probe switch, the 592-B eBPF frame and the key store stay. |
| M | the pass output's memory operation alone (`ld.u32` x2, add, `st.u32` x2) on `buf + 8 x tid` |
| M64 | the same with one `ld.u64` / `st.u64` |
| E | the real pass's exit probe for an empty eBPF program (`r0 = 0; exit`). ptxas dropped the unused probe-switch load, so E is CALL/RET only. |
| 8-B store, + `membar.sys` | step 0's `st_lane32` f0 / f1 (module k), the in-run anchors |

Terms, each a paired per-round difference of ns per thread execution:
- fence = F - N, and IF - I;
- helper call + dispatch = N - I;
- cold fallback present = IC - I;
- probe call = I - K;
- lookup overhead (switch, stack key, 3-D id, bounds, address) = K - M;
- memory op = M;
- u32 split = M - M64;
- read-modify-write over a plain store = M - 8-B store.

These costs overlap, so the terms are not expected to add up exactly. The report gives every level alone and every term.

### (b) Slot addressing, device memory, R = 1 and Rbig

Each slot is 16 B, `{tag, state}`. The tag is `(k+1)<<48 | r<<40 | global tid`.

| item | index | per iteration |
|---|---|---|
| slot_e | `(%smid x %nwarpid + %warpid) x 32 + %laneid`, bounds-checked against `%nsmid x %nwarpid x 32` (12,288 here, 192 KiB); out of range is counted | `st.global.v2.u64 {tag, 0}` |
| slot_ex | same | slot_e, then the exit side's instructions as a separate probe would run them: ids read again, index recomputed, `ld.global.v2.u64`, tag compared, mismatch counted. ptxas schedules the second id reads before the entry store (see Known limits) |
| slot_excg | same | slot_ex with `ld.global.cg` at exit (L2, not L1) |
| tid_e, tid_ex | global 1-D tid (no special registers; T slots) | as slot_e, slot_ex |
| tidw_e | tid & 16383 (256 KiB, about the hardware table's footprint; no special registers) | as slot_e |
| slot_cx | hardware index; table in the top 256 KiB of the buffer, which no other item writes | `atom.acquire.gpu.cas` 0 -> tag (failure counted, no slot), state cleared; exit: index recomputed, tag read with a strong 8-B `ld.relaxed.gpu` (state with a weak 8-B load) and compared, `st.release.gpu` of 0 |
| slot_cxr | same | slot_cx with relaxed CAS and store, same strong tag load (a cost probe for the ordering) |

Terms:
- special-register addressing = slot_e - tidw_e;
- exit check = slot_ex - slot_e, and slot_excg - slot_e;
- footprint = tid_e - tidw_e;
- claim/release over tag store = slot_cx - slot_ex;
- ordering = slot_cx - slot_cxr.

### (d) Fence kinds

| item | memory | what runs |
|---|---|---|
| `fence_lane32` | none, one item per shape: no memory is touched | `membar.sys`, `membar.gl`, `fence.acq_rel.gpu` and `fence.acq_rel.sys`, executed by every lane, which is one warp instruction when converged |
| `fence_lane1` | none | the same four fences, by lane 0 only |
| `st_lane32` | reg and dev | the store with no fence, `membar.sys`, `membar.gl`, `fence.acq_rel.gpu`, `fence.acq_rel.sys` |
| `st_lane1` | reg and dev | the store with no fence, `membar.sys`, `membar.gl` |

Values are per warp-op. The fence part = store with the fence minus store without it, paired per round.

### Validation (every item, before timing; `items.json`)

| items | check |
|---|---|
| ladder | every thread's own word = R (each of the T words is compared, and their sum = T x R) |
| X module | inline-path misses = 0 |
| stores | step 0's sample check |
| slot_ex, tid_ex | hits = T x R, misses = 0, out-of-range = 0 |
| slot_e, tidw_e | every used slot holds a well-formed tag of a valid tid; tidw_e uses exactly min(T, 16,384) slots |
| tid_e | sampled slots hold their tid's tag |
| slot_cx, slot_cxr | hits = T x R, 0 claim failures, every slot released afterwards |

Counters read after the rounds and reported in `items.json`: exit-check misses, out-of-range indices, claim failures and inline-path misses over all timed launches. All are expected to be 0.

### Controls that show the checks can fail

| control | what it must show |
|---|---|
| `slot_ex_badtag` (validated only) | the exit side expects `tag ^ 1`, so every check must miss: misses = T x R |
| `slot_cx_collide` (validated only) | lanes 2j and 2j+1 share a slot, so claim failures must be counted (> 0) and hits + misses = T x R |
| SASS controls (`sass_check.py`) | each check applied to a kernel that lacks what it looks for must reject it. The pairs: the gl check on the acq_rel kernel, the sys check on the gl kernel, the exit check on the entry-only kernel, the hardware-slot check on the tid kernel, the acquire/release check on the relaxed claim, the N check on F, the I check on IF, the K check on I, and the M check on K |
| per-module empty kernels | bound the launch overhead |
| loop / exit form of F and N at R = 1 | gives the ratio of the loop form to the exit form |

### SASS checks (`build/sass_check.json`, part of `build.sh`; `run_all.sh` refuses to run unless they all pass)

| kernel | the SASS must show |
|---|---|
| F, N | N has exactly one `MEMBAR.SC.SYS` less than F |
| E | 1 CALL, no memory instruction |
| I | 1 CALL (the probe), 0 MEMBAR, 2 x LD.E + 2 x ST.E, the probe-switch LDG, the key STL |
| IF | I's checks, with exactly 1 `MEMBAR.SC.SYS` |
| K | 0 CALL, and in the loop form the switch LDG and 2 x LD.E / ST.E inside the loop |
| M, M64 | 0 CALL, 0 MEMBAR, the memory operations inside the loop. M's loop advances by `8 x k` (k = 0 at run time), because with a constant address ptxas forwarded values across iterations. |
| slot items | `S2R SR_VIRTUALSMID` and `SR_VIRTID` twice inside the loop for entry + exit, `STG.E.128`, `LDG.E.128` (`.STRONG.GPU` for cg), the compare. The check counts instructions; it does not check their order (the exit pair is scheduled before the entry store) |
| tid items | no `SR_VIRTUALSMID` |
| claims | `ATOMG.E.CAS.64.STRONG.GPU`; the tag load `LDG.E.64.STRONG.GPU` and the weak state load `LDG.E.64` in the loop, no `LDG.E.128`; the acquire/release form adds `CCTL.IVALL` and `MEMBAR.ALL.GPU` before `STG.E.64.STRONG.GPU`, and the relaxed form has neither |
| fences | `membar.sys` = `MEMBAR.SC.SYS`, `membar.gl` = `MEMBAR.SC.GPU`, `fence.acq_rel.gpu` = `MEMBAR.ALL.GPU`, `fence.acq_rel.sys` = `MEMBAR.ALL.SYS`. Each is required in the loop and the other three are forbidden. `ERRBAR` and `CCTL.IVALL` are required in the loop too (ptxas emits them after every fence kind). |

Also seen in SASS:
- `%nsmid` compiles to a constant-bank load (`c[0x0][0x10c]`) and `%nwarpid` to the immediate 48.
- `__threadfence()` emits `membar.gl` and `__threadfence_system()` emits `membar.sys` (CUDA 13.2, sm_87).
- Every kernel of all four modules (81 of 81, counted in the .sass files) starts with the same guarded `BMOV.32.CLEAR B0-B15` prologue, `k_empty` and the `ref_empty` kernels included (module k has no CALL), so the per-module empty kernels include it.
- In slot_ex, slot_excg, slot_cx and slot_cxr, ptxas schedules the exit side's `S2R SR_VIRTUALSMID` / `SR_VIRTID` before the entry store or CAS of the same iteration (at every R, R = 1 included).

## Decision rules (written before any real run)

The thresholds and their projections:
- 10% of a pp512 = 0.1062 ns per thread execution;
- 6% = 0.0638 ns, the top of the raw-store band;
- 1% = 0.0106 ns.

[projection] % = 192,560,640 thread executions x ns / 204.6 ms, or 6.02M warp records x ns per warp / 204.6 ms (step 0's definitions). The primary reading is **anchor, device memory, R = 1 (exit form)**. large, full and registered memory are reported as secondary readings and decide nothing.

**Gate: comparable with step 0.** These in-run anchor values (anchor, R = 1) must lie within the tolerance of step 0's pass 1:

| item | step 0 | tolerance |
|---|---:|---:|
| F reg | 0.433 ns | 10% |
| F dev | 0.369 ns | 10% |
| 8-B store dev + `membar.sys` | 0.259 ns | 10% |
| 8-B store dev | 0.032 ns | 25% (an 8 us delta) |

F's loop / exit ratio must also lie in [0.8, 1.25]. N's ratio is reported as a sanity line and is not part of the gate, because step 0 never characterised N.

Otherwise the report says "not comparable with step 0". The within-run split still stands, but no value of it is combined with step 0's numbers. If a loop / exit ratio falls outside the range, run again before reading.

**Run validity** (added in review, before any real run). Every verdict of (a), (b) and (d) carries a bracketed prefix, and decides nothing, unless the run is valid: 0 validation failures, every requested round done, at least 10 rounds, and 0 timed inline-path misses. A smoke therefore always prints "NOT A MEASUREMENT". `run_all.sh` exits with bench3's status when that is not 0 (6 = a validation failed, 124/137 = timeout).

**(a) Split.** Every level and term is reported in ns and %. A term is "resolved" if its paired difference has the same sign in every round. Then, at the primary reading:

| rule | condition | statement |
|---|---|---|
| A1 | N projects >= 10% | "the fence-free helper path still projects >= 10%" |
| A2 | N <= 6% | "raw-store band reachable without inlining: removing the fence suffices" (a cost statement; whether the fence may be removed is a separate memory-model question) |
| A2 | else, I or IC <= 6% | "reachable only with an inlined helper path" (both values given) |
| A2 | else, K <= 6% | "reachable only with the probe body inlined into the kernel as well" |
| A2 | else, M <= 6% | "reachable only with the op reduced to its memory access (no probe call, switch, stack key, 3-D id or bounds checks)" |
| A2 | otherwise | "not reachable even by the memory access alone", with M - 8-B store given as the read-modify-write's share |
| A3 | the round spread of N, I, IC, K or M crosses 6% or 10% | mark "unstable" |
| A4 | (IF - I) / (F - N) outside [0.7, 1.3] | "fence cost is not additive across paths" |

Rule A2 also lists every level that is single digit (< 10%).

**(b)** At the primary reading:

| rule | measure | statement |
|---|---|---|
| B1 | special-register addressing (slot_e - tidw_e) | "negligible" if < 1% (0.0106 ns), "visible" otherwise |
| B2 | the entry + exit tag protocol (slot_ex) | "within the raw-store band" (<= 6%), "single digit" (< 10%) or ">= 10%" |
| B3 | the claim-based protocol (slot_cx) | the same classes as B2. It is the cost of the form built from documented primitives; whether that protocol is sufficient is a memory-model argument this check does not make. |
| B4 | the two validation controls | if either fails, B1-B3 carry the prefix "VOID: a slot control did not fire" |
| B5 | misses, out-of-range or claim failures in the timed launches | reported as a finding: a collision (another thread's tag in the slot) or an out-of-range index was detected and counted. They are not a harness failure. Because the exit side's ids are read together with the entry's (Known limits), a miss cannot come from an id change, and zero misses says nothing about id stability. |

**(d)** For each shape, memory, and lane-32 / lane-0 case:
- **D1, after a store:** the fence part of `membar.gl` against `membar.sys`, cheaper-by = (sys - gl) / sys on the medians. "about the same" if |cheaper-by| <= 10%. Otherwise the paired per-round difference sys - gl must have the same sign in every round, else "unresolved". If it does: "membar.gl materially cheaper" if cheaper-by >= 25%, "somewhat cheaper" between 10% and 25%, "membar.gl dearer" below -10%. If the sys part's median is not positive: "unresolved". (The resolution requirement and the "dearer" class were added in review, before any real run.)
- **D2, alone:** the same classes on the fence alone.
- acq_rel against sc is reported without a rule.

## Documented guarantees each mechanism relies on

Quoted from the PTX ISA 9.5 HTML, `docs.nvidia.com/cuda/parallel-thread-execution`, fetched 2026-11-15. It is newer than the PTX ISA that CUDA 13.2 implements, and the sentences quoted below are old ones (registers since PTX 1.3/2.0, the membar synonym since sm_70).

| mechanism | what the documentation says | what C3 therefore relies on |
|---|---|---|
| Slot index from `%smid` / `%warpid` / `%laneid` | `%laneid` "ranges from zero to WARP_SZ-1". `%smid` "ranges from 0 to %nsmid-1. The SM identifier numbering is not guaranteed to be contiguous". `%nwarpid` "returns the maximum number of warp identifiers". `%warpid` "provides a unique warp number within a CTA but not across CTAs within a grid", and for both: "its value may change during execution, e.g., due to rescheduling of threads following preemption". | The bound of the index is documented for `%smid` and `%laneid`, and implied by the name for `%warpid` (`< %nwarpid`); the run-time bounds check with a counted fallback covers it anyway. **Exclusivity of a hardware slot among co-resident warps of different CTAs is not documented.** It was measured in step 0 (e1). **Stability between entry and exit is not documented either**, so the exit tag check with a counted miss is required. |
| Plain tag protocol (slot_e / slot_ex) | none for exclusivity | Correct only while slots are exclusive: measured behaviour. As a protocol, its tag check detects a slot that changed owner before the exit check; it does not detect a second owner's state writes landing in between, and its weak 16-B exit load has no single-copy atomicity against another thread's store (8.10.3). In this measurement the check runs at once (Known limits). So it is a performance figure for the case where the measured exclusivity holds. |
| Claim protocol (slot_cx), built from documented primitives | CAS against CAS on the tag: 8.7.2, "Atomicity of mixed-size RMW operations": for overlapping atomics whose scopes include each other, "Either the read-modify-write operation specified by A1 is performed completely before A2 is initiated, or vice versa". CAS against the release store and the exit's tag load: 8.10.3 "Atomicity", which holds for morally strong operations ("Conflicting morally strong operations are performed with single-copy atomicity"; "Atomicity is guaranteed when the operations are morally strong"); 8.7: two operations are morally strong when they are in program order of one thread "or each operation is strong and specifies a scope that includes the thread executing the other operation", "Both operations are performed via the same proxy" and "they overlap completely". The exit's tag load is therefore a strong 8-B `ld.relaxed.gpu` (a weak load racing with another thread's CAS or release store would have no single-copy atomicity). 8.2.4: a vector access is modelled as scalar accesses of at most 64 bits in unspecified order. Release and acquire patterns, 8.8: "A release pattern on a location M consists of ... A release operation on M, E.g.: st.release [M]" and "An acquire pattern ... An acquire operation on M, E.g.: ... atom.acquire [M]" | The claim, the tag check and the hand-over between owners use only these primitives. **That the protocol is sufficient under the memory model is not argued here**: for example, the state clear after the claim is a plain `st.global`, and no argument is made that the patterns cover it. C3 gives its cost. If the ids change, the result is a counted miss and a slot that stays claimed: a leak. Reclaiming leaked slots is a design question, not tested here. |
| `membar.gl` / `membar.sys` | "On sm_70 and higher membar is a synonym for fence.sc, and the membar levels cta, gl and sys are synonymous with the fence scopes cta, gpu and sys respectively." "fence.acq_rel is a light-weight fence that is sufficient for memory synchronization in most programs. Instances of fence.acq_rel synchronize when combined with additional memory operations as described in acquire and release patterns" | (d) times these instructions; it decides nothing about which one is sufficient for a given publication protocol. That is a memory-model argument per protocol. |
| Everything C3 measures | - | **Per-platform performance parameters only** (Orin, sm_87, this driver). The fence-free variants (N, I, K, M) are cost probes, **not evidence that the 1512 fence may be dropped**. Whether that fence is needed is a separate memory-model question; the trampoline's comment says it is there to "Ensure CPU writes are visible to GPU". |

## Commands (main session, quiet board, clocks checked)

```
cd /home/jetson/src/bpftime-gpu-work/step0b/c3-costparts
./build.sh                  # CPU only, about 18 s: kernels, refF/refN/refX, SASS checks, bench3, diffs
./run_all.sh 10             # pass 1: results/run-<time>/ ; timeout -k 5 300 around bench3
./run_all.sh 10             # pass 2, serially after pass 1
python3 -I compare_runs.py results/run-<pass1> results/run-<pass2> results/compare-passes.md
python3 -I threadend/threadend.py threadend/out   # (c), CPU only, < 1 s; already run
```

- **Expected wall time:** about 25 s per `run_all.sh 10`: about 8 s of validation and calibration, then about 1.3 s per round. 324 items (318 timed, 6 validated only), largest about 2.8 ms per launch.
- **What `run_all.sh` does:** it refuses to start unless the SASS check printed ALL OK. It logs the clocks and GPU load before and after, `/dev/shm` (`step0b_` names) before and after, and the sha256 of the binaries before the run. It writes `raw.csv`, `items.json`, `summary.csv`, `derived.json`, `summary.md` (its "Run status" line comes first) and `analyze.log`, and exits with bench3's status when that is not 0. It does not check or set clocks: check `jetson_clocks` state (cpu5 at 1,728,000 kHz) and that no other GPU process runs before each pass.
- **Options:** `bench3` takes `--shapes`, `--only REGEX`, `--rounds`, `--rbig`, `--no-hold` and `--max-wall-s`.

## Known limits

- **Empty kernels.** The cost is fully exposed, as in step 0. In real kernels the memory operation can overlap with work or compete with it.
- **The inlined variants are hand-written from the helper's PTX**, not output of a modified pass. A JIT that inlined the helper could also fold the constant key (0) and the 3-D thread id of a 1-D launch, so I and K are upper bounds for an inlined path built the same way. IC's fallback is counted and calls the real helper; the cost of saving registers around it shows up as IC - I.
- **E is CALL/RET only**, because ptxas removed the probe-switch load of the empty program. The switch load is inside I - K.
- **Loop forms (Rbig).**
  - ptxas hoisted the `%laneid` reads out of the R loop in slot_e, slot_ex and slot_excg, and one of the two in the claim kernels, so their slopes are slightly optimistic.
  - M's loop needs the opaque stride (see SASS checks).
  - The slopes are secondary readings and decide nothing.
- **The exit side's id reads are not after the entry.** In slot_ex, slot_excg, slot_cx and slot_cxr, ptxas schedules the exit side's `S2R SR_VIRTUALSMID` / `SR_VIRTID` before the entry `STG.E.128` or CAS of the same iteration, at every R (R = 1 included). All instructions are there and are timed, but the second read's latency overlaps the entry's, so these items are slightly optimistic, and they cannot observe an id change between entry and exit. No barrier was added, because it would time an instruction the protocol does not have. A real exit probe runs in a separate call after the kernel body.
- **The exit check runs right after the entry store**, so its load finds the line in L1 or L2. In a real kernel the body runs in between. slot_excg forces L2; a DRAM miss is not covered.
- **The slot state is 16 B.** Larger state adds stores at entry.
- **The claim form** releases with a plain `st.release.gpu` and never reclaims leaked slots.
- **Fence alone** touches no memory, so it is one item per shape. Fences after stores to the other memory kind, or after many outstanding stores of other warps, are not covered.
- **The projection** applies one shape's cost to all 444 pp512 launches, as in step 0, and is labelled [projection].
- **Concurrency.** Valid only with no other GPU process.

## (c) Thread-end operations from step 0's data (no new runs)

`threadend/threadend.py` reads only step 0's two real-run `summary.csv` files (`e2-cost/results/run-20261115-050452` and `run-20261115-060441`) and writes to `threadend/out/` (`threadend.md`, `threadend.csv`, `threadend.json`). Its self-check reproduces step 0's README: 0.0321 ns / 3.02%, 0.4329 ns / 40.74%, 10.06 ns / 29.60%.

The case definitions, normalisation and the reason k < 48 gets no projection are in the script's header. The table below is pass 1. Over the 68 non-tiny R = 1 rows other than atom_sm k < 48, pass 2 / pass 1 is 0.85-1.04. Four of them moved by more than 5%: red_warp_leader dev f0 at full (0.87), red_warp_leader reg f1 at full (0.95), and atom_warp_leader / atom_warp_leader_sys dev f0 at large (0.85). With the 24 atom_sm k < 48 rows (92 rows), the range is the same and two more moved by more than 5%: atom_sm dev f0 k = 1 (0.93) and dev f1 k = 8 (0.95), both at full. Step 0 ran the atom_sm k-sweep only at full and large, so the anchor columns of the atom_sm rows are empty.

[projection] % of a pp512 if every warp did the operation once per kernel. R = 1, pass 1, ns per warp in parentheses. tiny (2 warps) is left out because it measures one launch's latency.

| operation | dev anchor | dev large | dev full | reg anchor | reg large | reg full |
|---|---:|---:|---:|---:|---:|---:|
| per-warp merge, red (result unused) | 2.7% (0.93) | 2.2% (0.76) | 14.9% (5.06) | 51.9% (17.64) | 52.2% (17.74) | 130.8% (44.47) |
| per-warp merge, red + membar.sys | 22.3% (7.57) | 23.5% (7.98) | 26.9% (9.16) | 127.8% (43.44) | 124.8% (42.40) | 171.2% (58.17) |
| per-warp merge, atom (result used) | 4.6% (1.57) | 6.1% (2.06) | 16.4% (5.56) | 148.7% (50.54) | 140.5% (47.75) | 163.8% (55.68) |
| per-warp merge, atom.sys | 4.6% (1.57) | 6.1% (2.06) | 16.3% (5.55) | 148.7% (50.55) | 140.3% (47.68) | 163.8% (55.68) |
| per-lane atom on its own word (32 per warp) | 5.4% (1.82) | 14.4% (4.91) | 18.4% (6.25) | 27.4% (9.31) | 25.9% (8.81) | 46.1% (15.66) |
| per-SM merge word, every warp (atom_sm k = 48) | - | 5.2% (1.78) | 16.4% (5.57) | - | 863.9% (293.60) | 314.8% (107.00) |
| per-SM merge word + membar.sys (k = 48) | - | 23.3% (7.91) | 29.3% (9.96) | - | 670.6% (227.90) | 291.3% (98.99) |
| record write-out, per-SM ring, no fence (comp_rec) | 7.7% (2.61) | 7.0% (2.39) | 20.9% (7.10) | 330.7% (112.40) | 324.0% (110.10) | 381.0% (129.50) |
| record write-out, per-SM ring, membar.sys (comp_rec) | 27.1% (9.22) | 25.8% (8.78) | 39.0% (13.26) | 623.2% (211.80) | 637.9% (216.80) | 383.4% (130.30) |

What the table shows:
- **Device memory, anchor and large:**
  - A per-warp merge with `red` projects 2.2-2.7%, and with `atom` 4.6-6.1%.
  - A merge into one word per SM by every warp projects 5.2% (large).
  - An unfenced ring record projects 7.0-7.7%.
  - Any `membar.sys` puts each of them at 22-27%.
- **Registered memory:** every merge or write-out projects about 26% or more (the per-lane atom 25.9-27.4%). Per-warp atomics project 140-149% and the per-SM word 315-864%; atomics on registered memory serialize, as step 0 found.
- **Full occupancy** (24 x 512): every unfenced device-memory row is 15-21%. Its deltas are 2-4 us, near the launch floor, so this is the least precise column.
- Per-SM word, k < 48, and the R-slopes: in `threadend/out/threadend.md`.

## Files

| path | contents |
|---|---|
| `src/kernels.cu` | step 0's kernels + C3 cases |
| `src/bench3.cpp` | the host program |
| `src/step0b_regmem.h` | step 0's `step0_regmem.h` with the `step0b_` prefix |
| `ref/build_variants.py` | modules F, N, X |
| `sass_check.py` | SASS checks and controls |
| `analyze.py` | (a), (b), (d) and the decision rules |
| `compare_runs.py` | pass 1 against pass 2 |
| `threadend/threadend.py`, `threadend/out/` | (c) |
| `build.sh`, `run_all.sh` | build and run |
| `diffs/` | every change against step 0's originals, for review |
| `build/` | cubins, kept PTX (`build/kernels.ptx`, `build/ref/ref{F,N,X}.ptx`), SASS, `sass_check.json`, `ref/variants.meta.json` (input hashes, the identity check, the removed fence) |
| `results/smoke-*` | **smokes, not measurements** (1-2 rounds, shared GPU) |

## Review changes (before any real run)

- Ladder validation compares every thread's word with R; before, only the sum was compared, although this README already said "every thread's word".
- slot_cx / slot_cxr read the tag at exit with a strong 8-B `ld.relaxed.gpu` and the state with a weak 8-B load, instead of one weak 16-B load: a weak load racing with another thread's CAS or release store is not single-copy atomic (PTX ISA 8.10.3), which the "documented primitives" form must not rely on. SASS checks updated (`LDG.E.64.STRONG.GPU`, no `LDG.E.128`).
- SASS checks now require `ERRBAR` and `CCTL.IVALL` after each fence kind, as the table already stated.
- `analyze.py`: verdicts carry a run-status prefix (validation failures, incomplete or fewer than 10 rounds, timed inline misses), B4 voids B1-B3, B5 is a finding line, (d) gets the "unresolved" and "dearer" classes.
- `run_all.sh` exits with bench3's status when it is not 0.
- Corrected: the exit side's id reads are scheduled before the entry store (Known limits); "R = 1 is exact" was wrong. The (c) pass-ratio sentence now says which rows it covers.
- `results/smoke-20261115-223033` predates these changes. `results/smoke-20261115-230135` is the review's 1-round smoke of the current build (binaries identical by sha256; shared GPU, not a measurement): 0 validation failures, both slot controls fired, every timed counter 0, no `step0b_` segment left, about 10 s.

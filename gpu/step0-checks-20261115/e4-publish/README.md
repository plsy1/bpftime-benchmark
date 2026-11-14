# e4-publish: CPU/GPU publication in registered memory (step 0, check 4)

Design note: `/home/jetson/src/results/gpu/dir1-design-space-20261115/README.md`, section 5, check 4:

> **CPU/GPU publication test in registered memory** (A's drain, C, D).
> - Lanes write a payload, `bar.warp.sync`, one leader `membar.sys`, then a flag. The CPU acquire-reads the flag, then the payload, for 10^8 rounds.
> - Also: a CPU release-store, then a GPU CAS on the same word.
> - Decides: one fence per warp, or one per lane (today's cost); C's state machine; A's accept rule.

Status: built and smoke-tested only. The smoke runs had 10^6 to 2 x 10^7 rounds per variant. **No real run has been made.** The real run is `run_all.sh` on a quiet board.

## Method

`build/pub4` (`src/pub4.cu`) uses the runtime API for kernels. Registered memory is made the bpftime way (`../common/step0_regmem.h`): `shm_open("step0_e4_<pid>")`, `ftruncate`, `mmap MAP_SHARED`, read-prefault, `cuMemHostRegister(DEVICEMAP|PORTABLE)`, `cuMemHostGetDevicePointer`.

The CPU threads use a **second, unregistered MAP_SHARED mapping** of the same object, as the loader process would. The name is unlinked once both mappings exist. Grid: 48 x 256 = 384 warps, all resident (full occupancy).

**Test 1: GPU publishes, CPU consumes.**
- Each warp owns a ring of S = 4 slots. A slot is a flag line, an ack line (128 B each) and the payload.
- In round r, warp w uses slot (r-1) % 4:
  1. Lane 0 waits until `ack == r - 4`, with plain volatile loads (`ld.volatile`, as bpftime's flag2 wait does), so no variant has a fence in its wait. The wait is capped at 2^26 polls and by a globaltimer deadline; cap hits are counted, and the warp then stops without publishing.
  2. `__syncwarp`.
  3. Every lane writes its words, v = (r << 32) | (w << 16) | (lane << 8) | word.
  4. `__syncwarp`.
  5. The variant publishes:

| variant | publish | SASS in the round loop (`build/sass_check.json`) |
|---|---|---|
| A (the protocol) | lane 0: `membar.sys; st.relaxed.sys flag` | `MEMBAR.SC.SYS; ERRBAR; CCTL.IVALL; STG.E.64.STRONG.SYS` |
| B (today's per-lane cost) | every lane `membar.sys` before `__syncwarp`; lane 0: `st.relaxed.sys flag` | same instructions, MEMBAR executed by the full warp |
| C (control) | lane 0: `st.relaxed.sys flag`, no fence | no MEMBAR, no CCTL |
| Cd (delay control) | as C, but lane 0 first spins on globaltimer for `--delay-ns`, no fence. `run_all.sh` sets the delay to the median duration of A's fence in the first round of a launch, measured by Am in the same mode (smokes: 28 us stress, 7 us normal; the earlier fixed 1.5 us came from e2's 64-thread smoke and was 5-20 times too short) | no MEMBAR, no CCTL; mean delay is reported |
| Am (calibration) | A, with lane 0's `membar.sys` timed by `%globaltimer` reads right before and after it; durations go to two histograms (first round of a launch, other rounds; 64-ns bins up to 262 us), updated after the flag store | as A, plus `CS2R SR_GLOBALTIMERLO` on both sides of `MEMBAR.SC.SYS; ERRBAR; CCTL.IVALL`, before the flag store (checked) |
| D1 | lane 0: `st.release.sys flag` | `MEMBAR.ALL.SYS; ERRBAR; STG.E.64.STRONG.SYS` (no CCTL), distinct from A |
| D2 | lane 0: `fence.acq_rel.sys; st.relaxed.sys flag` | `MEMBAR.ALL.SYS; ERRBAR; CCTL.IVALL; STG...STRONG.SYS` |
| E (must fail) | lane 0 stores the flag **before** the lanes write | flag store precedes the payload store |

- **Publisher without payload (`--lane0-idle`, runs A0, C0, Am0mix, stress mode).** Lane 0 writes no payload word; lanes 1-31 write theirs, then `__syncwarp` (now a diverged `WARPSYNC`), then lane 0 publishes as in A (C). In A, lane 0's own stores are in the same store instructions as the other lanes', so a fence that covered only the issuing lane's accesses would still wait for transactions issued together with the others' and would rarely lose the race: C's failure rate overstates A's power against that hypothesis. With lane 0 idle, such a fence would order nothing of the payload, so C0 (same layout, no fence) is the power reference.
- **Fence baseline (`--odd-warps-idle`, run Am0mix).** One Am run in which even warps are laid out as A0 and odd warps write no payload at all; their fence durations go to separate histograms, under the same load. If a fence in a warp without stores takes about as long as the leader's fence in A0's layout, the fence's duration does not come from waiting for its own warp's stores, and A0's zero cannot be separated from that duration (a fence that ordered nothing of the payload would have hidden the race too, as the calibrated Cd shows for A).
- On sm_87, a converged `__syncwarp` compiles to `BRA.CONV` plus a `WARPSYNC` taken only when the warp is diverged. There is no barrier instruction in the converged path.
- The PTX of every variant still has its `bar.warp.sync` instructions: `sass_check.py` counts them, and `membar.sys`, `st.release.sys`, `fence.acq_rel.sys` and `atom.acquire`, per kernel in `build/pub4.ptx`.
- **CPU readers** (4 threads, warps split by w % 4):
  - `__atomic_load_n(&flag, __ATOMIC_ACQUIRE)` (LDAR) == r.
  - Read every payload word (volatile) and check it.
  - Optionally overwrite it (poison), then `__atomic_store_n(&ack, r, __ATOMIC_RELEASE)` (STLR).
  - A flag value other than r or r - 4 is an anomaly.
- **Mismatches** are classified as stale (older round), poison (consumed data), future (newer round: the GPU overwrote early) or other.
  - The first 50 per reader are re-read after 1, 2, 4 ... 512 ms. Healed means the words arrived late: an ordering violation. Never healed means a test bug.
  - Time-slicing with another GPU process can stop a warp between two instructions for whole slices, hence the long re-read schedule.
- **Modes:**
  - stress (`--stress --poison`): every lane writes 4 words spread at 16 B. The payload is 16 GPU lines (2 KiB) from 4 store instructions. Readers read it in reverse order, then dirty every payload line (poison) before the ack.
  - normal: 1 word per lane, 256 B, no poison.
- **Launch chunking:** `--chunk-rounds 4` starts a new launch every 4 rounds, so every slot of every warp is written once per launch.
  - In the smokes, almost every reordering seen in C and E fell in **the first round of a launch** (C: 718 of 720; E: 1,756 of 1,756; the 3 C failures in long mode were at rounds 65, 321 and 1,345, the starts of launches 2-4). The boundary is where the test has power. The cause is not established (see Known limits).
  - `--chunk-s 1` (long launches, about 1 s) tests the steady state inside a launch. With few launch starts it is expected not to discriminate, so it gets 10^8 rounds, not 10^9.
- Each launch has a globaltimer budget (at least 5 s) and the whole run a `--budget-s`.

**Test 2: CPU releases, GPU claims with CAS.**
- 64 slots: a state word (round << 2 | FREE/WRITING/FULL/STOP), a winners word, a header and a 32-word payload (128 words in `--stress`).
- **CPU** (4 threads, slots split): on FULL(r) (acquire load):
  1. Check the GPU payload against gpu_pat(r, writer) and check winners == 1.
  2. Set winners = 0.
  3. Reset the payload to cpu_pat(r + 1).
  4. `__atomic_store_n(state, FREE(r + 1), RELEASE)`.
  5. After the last round, store STOP instead.
- **GPU** (384 warps over 64 slots; lane 0 claims):
  1. `ld.volatile` state; if FREE, CAS FREE -> WRITING.
  2. The winner `atomicAdd_system(winners)`; a non-zero result is a double winner.
  3. The variant's acquire, `__syncwarp`.
  4. Every lane reads its payload words (plain `ld.global`) and counts those that are not cpu_pat(r): stale.
  5. The lanes write gpu_pat(r), `__syncwarp`.
  6. Lane 0 `membar.sys; st.relaxed.sys` FULL(r).
  - The loop is capped at 2^30 iterations plus a globaltimer deadline per launch.
- **Variants:**

| variant | GPU acquire | SASS |
|---|---|---|
| cas | `atomicCAS` (`atom.cas`, .gpu scope, relaxed), no fence | `ATOMG.E.CAS.64.STRONG.GPU` |
| casfence | `atomicCAS_system` + lane 0 `membar.sys` | `ATOMG...STRONG.SYS; MEMBAR.SC.SYS; ERRBAR; CCTL.IVALL` |
| casacq | `atom.acquire.sys.cas` | `ATOMG...STRONG.SYS; CCTL.IVALL` (no MEMBAR) |

- **Control** (`--wrong-order`): the CPU stores FREE first, waits 50 us, then resets the payload. Stale reads must appear.
  - Smoke (`results/run-20261115-040157-smoke`): cas 6,252 stale rounds of 20,032; casfence 5.
  - In the control, the GPU payload check fails too, because the late reset overwrites the GPU's words. That count is meaningless there.

## Decision rules (written before any real run)

**Validity of a test-1 mode** (stress, normal, long). The mode's verdict is INVALID if any of the following holds:
- E shows no violation. The checker cannot see stale data; for long, the E run of the normal mode is used.
- Any variant has a violation that never healed. That is a harness bug.
- Any variant has a flag anomaly, a future word or an other word.
- Published rounds were not all verified.

**Test 1 verdict per mode**, with N_A rounds of A and V_A violating rounds:
- V_A > 0: **one leader fence is not sufficient** (observed). Designs need a fence per lane (B) or another publish.
- V_A = 0 and C has V_C > 0 in the same mode:
  - **"no violation observed in N_A rounds"**, never "proven". The 95% upper bound on A's per-round rate is 3/N_A.
  - It counts as **evidence for one fence per warp** only if A had the power to fail: p_C x N_A >= 3, with p_C = V_C/N_C. Otherwise it is "consistent, under-powered".
- V_A = 0 and V_C = 0: **inconclusive-but-consistent**. The setup cannot discriminate.
- **The A verdict's scope.** "Evidence" here means that one leader fence after `__syncwarp` suffices in A's layout, where the publisher's own payload stores share instructions with the other lanes'. Whether a leader fence orders stores of lanes that did not fence is decided by the lane-0-idle rule below.
- **Attribution (Cd)**, when V_A = 0 and V_C > 0 in a mode (stress, normal). Cd's delay must be calibrated: its mean delay at least 0.9 x Am's median fence duration in the first round of a launch, same mode; otherwise "undecided (uncalibrated)".
  - V_Cd >= 3 and p_Cd x N_A >= 3: **the fence's ordering, not only its duration**: a fenceless wait as long as the fence still fails, and A would have expected at least 3 failures if its fence were only a delay.
  - 0 < V_Cd, but either condition fails: undecided (too few Cd failures).
  - V_Cd = 0 with p_C x N_Cd >= 3: **the fence's duration**: a fenceless wait as long as the fence hides the race; A is "safe in practice here, not attributable to fence semantics".
  - V_Cd = 0 with fewer rounds: undecided.
  - A fence that works by waiting until its stores are performed is itself a wait, so "duration" does not mean the fence is useless; it means this setup cannot separate its ordering from its timing.
  - Smoke with the uncalibrated 1.5 us delay (`results/run-20261115-040157-smoke`): stress Cd 645 of 10^6, which the earlier rule read as "the fence". Re-smoked with the calibrated delay (`results/run-20261115-044555-smoke`): stress Cd 0 of 10^6 at 29.3 us (C 1,345), normal Cd 0 of 10^6 at 6.9 us (C 166), so "the fence's duration" in both modes. A 1.5 us wait still failed 224 times in 10^6 (stress) and 0 times (normal).
- **Lane 0 idle (A0, C0, Am0mix; stress mode).** Validity as above (E stress must fail; no persistent violation, anomaly, future or other word, or unverified round in A0, C0, Am0mix). Let F = Am0mix's median first-round fence duration in warps laid out as A0, F0 the same in warps without payload, ratio = F0 / F.
  - V_A0 > 0: **a leader fence does not order the other lanes' stores** (observed).
  - V_A0 = 0, V_C0 > 0, p_C0 x N_A0 >= 3 and ratio <= 0.5: **evidence that one leader fence orders the stores of lanes that did not fence**, on this GPU ("no violation observed in N_A0 rounds", 95% bound 3/N_A0): the leader's fence takes at least twice as long when its warp has stores outstanding, so it waits for stores it did not issue.
  - The same with ratio > 0.5 (or Am0mix missing): **no violation observed in N_A0 rounds, not separable from the fence's duration**. A fence in a warp with nothing to wait for takes about as long under this load, so the zero says nothing about whether the fence orders other lanes' stores; one fence per warp is then "safe in practice here", as for A.
  - V_A0 = 0 with fewer expected C0-like failures: consistent, under-powered. V_C0 = 0: inconclusive-but-consistent.

**Other variants.**
- B is expected to show 0. B > 0 is flagged for investigation.
- D1 and D2 are reported the same way as A. D1 compiles to a different fence (`MEMBAR.ALL.SYS`, no L1 invalidation), so it is a distinct test.
- Rates of A and B (slot-rounds/s) are reported as rates, not as a timing claim. Readers, polling and the fence's loaded duration (Am) dominate them.

**Test 2.**
- The control must show stale rounds > 0, or the stale check cannot detect.
- Per variant:
  - Any double winner (the winner's `atomicAdd_system` on the winners word returned non-zero): **C's state machine is broken**.
  - The CPU found winners != 1 at FULL (the CPU's reset of the winners word, a plain store before the release of FREE, and the winner's add were not ordered): **that GPU acquire is insufficient**. Until this review it was counted as a double winner.
  - Stale rounds > 0: **that GPU acquire is insufficient**.
  - Otherwise: "no stale read and no double winner in N rounds" (bound 3/N).
- The control's artificial 50 us gap shows only that detection works. It does not measure how likely a real race is, so no power claim is made for test 2.
- A failing CPU check of the GPU payload, outside the control, means the GPU-to-CPU direction of C's chunk publish failed.

## PTX memory-model reasoning (PTX ISA 9.5; the verdict comes from the data)

Sections are those of PTX ISA Release 9.5 (docs.nvidia.com PDF, read 2026-11-15; text extracted locally).

1. **Scope of the model.** §8.1: the model applies to sm_70 or later. §8.1.1: "certain strong operations with system scope may not be performed atomically on some systems" when communicating with the host CPU. The Orin reports hostNativeAtomicSupported = 1.
2. **Program order is per thread.** §8.9.1 says program order "does not relate operations from different threads". Lane i's payload store and lane 0's fence are therefore not ordered by program order.
3. **Synchronization between threads** (§8.9.4) is created only by:
   - a fence.sc pair in Fence-SC order;
   - `bar{.cta}.sync/.red/.arrive` on the same barrier;
   - `barrier.cluster`;
   - a release pattern with a morally strong acquire pattern;
   - CUDA API events.
   **`bar.warp.sync` is not in that list**, and §8.5 notes that "the warp is not a scope". The only text on its memory effect is in the instruction (§9.7.15.2): "bar.warp.sync also guarantee memory ordering among threads participating in barrier. Thus, threads within warp that wish to communicate via memory can store to memory, execute bar.warp.sync, and then safely read values stored by other threads in warp."
4. **Release pattern and cumulativity.**
   - §8.8 item 3: a release pattern on M is "a release or acquire-release memory fence followed by a strong write on M in program order". `membar.sys` is `fence.sc.sys` on sm_70+ (§9.7.15.4: "membar is a synonym for fence.sc"). The flag store is `st.relaxed.sys`, a strong write. So A's lane 0 forms a release pattern.
   - §8.8: its effect "only affects operations occurring in program order before the first instruction in that pattern". That is lane 0's own operations, "further extended to operations in other threads through the transitive nature of causality order" (footnote 1).
   - §8.9.5: base causality order is transitive over program order and synchronizes-with. Its footnote: "The transitivity of base causality order accounts for the 'cumulativity' of synchronizing operations."
5. **What A therefore needs.** The chain for lane i ≠ 0 is: W_i (payload store) -> lane i's `bar.warp.sync` -> lane 0's operations after the barrier -> fence.sc -> flag store.
   - A synchronizes-with edge exists only if `bar.warp.sync`'s instruction-level guarantee is read as a §8.9.4 barrier synchronization at warp granularity.
   - With that reading: W_i precedes the flag store in causality order. The CPU's acquire load reads the flag; the host is in .sys scope (§8.5, Table 21), and we assume its LDAR is morally strong with the flag store. The Causality axiom (§8.10.6, item 2, the MP litmus test) then forbids the CPU's later payload read from returning a value older than W_i. One leader fence is then sufficient.
   - Without it, the formal model gives no order from W_i to the flag. §9.7.15.4 states membar's guarantee per thread: it "guarantees that prior memory accesses requested by this thread ... are performed at the specified level". Lane 0's fence therefore covers lane 0's accesses, and the other lanes' stores only through causality order.
   - **B is not better covered formally.** Its per-lane fences are ordered against lane 0's fence only through Fence-SC order (§8.9.3), and Fence-SC order is fixed at run time. B's per-lane fence before the barrier helps only through the same barrier edge.
   - A, B and D therefore all rest on the `bar.warp.sync` guarantee. B is the conservative variant today's 1512 code effectively pays for.
   - **The data decide on this hardware.**
6. **D1 and D2.** `st.release.sys` is a release pattern by item 1, and `fence.acq_rel.sys; st.relaxed.sys` by item 3, with the same cumulativity argument as A. §9.7.15.4 calls fence.acq_rel "a light-weight fence", and on sm_87 it compiles to `MEMBAR.ALL.SYS` instead of A's `MEMBAR.SC.SYS`.
7. **The hardware path** (SASS, not the model).
   - In the converged case `__syncwarp` emits no barrier instruction.
   - The payload stores of all lanes are one warp instruction (`STG.E.64`).
   - Lane 0's `MEMBAR` is issued by the same warp, so whether A works on this GPU depends on MEMBAR waiting for the warp's earlier stores, not only lane 0's share. The test measures exactly this.
8. **Test 2.**
   - `atomicCAS` is `atom.cas` with `.relaxed` and `.gpu` scope by default (§9.7.15.5: "If the .sem qualifier is absent, .relaxed is assumed by default"; "If the .scope qualifier is absent, .gpu scope is assumed by default").
   - Its read is therefore not an acquire pattern, and its .gpu scope does not include the host thread (§8.7 condition 1). Formally `cas` does not synchronize with the CPU's release store; it relies on how the hardware behaves.
   - `casfence` (`atom.sys` then `fence.sc.sys`) is an acquire pattern by §8.8 acquire item 3. `casacq` (`atom.acquire.sys`) is one by item 1.
   - The other lanes' payload reads after `__syncwarp` again depend on the `bar.warp.sync` guarantee (point 5, reverse direction).
9. **CPU side.** The CPU's acquire load and release store are C++/AArch64 (LDAR/STLR). The PTX model covers host threads only as members of .sys scope (§8.5). How LDAR/STLR map to its strong operations is not specified there; that is an assumption.

## Commands (main session; quiet board; nothing else on the GPU)

```
cd /home/jetson/src/bpftime-gpu-work/step0/e4-publish
./build.sh                 # CPU only: pub4, kept PTX/SASS, SASS check (about 15 s)
./run_all.sh full          # results/run-<time>-full/; every call: --budget-s <= 600, timeout -k 5 budget+60
#   ./run_all.sh quick     # about 1/10 of the rounds
```

- Single call example: `timeout -k 5 660 ./build/pub4 --test 1 --variant A --stress --poison --chunk-rounds 4 --rounds 1000000000 --budget-s 600 --out a.json`
- `run_all.sh` records the clocks, GPU load and `/dev/shm` before and after, then runs `analyze.py`, which writes `summary.csv`, `verdicts.json` and `summary.md`.
- Worst case: the per-call budgets add up to 7,200 s, reached only at rates far below the smokes' (another GPU process, for instance). `run_all.sh` therefore bounds itself: `TOTAL_S` (default 2,400 s) cuts each call's budget to what is left and skips the remaining calls (logged as SKIPPED in `meta.txt`) once fewer than 90 s remain. A call that hit its budget says `budget_hit: true`; its verdict rests on fewer rounds. Do not wrap the script in an outer `timeout`: it would kill the shell and leave the current `pub4` running until its own bound.

**Sizes and expected wall time** (full profile, from smoke rates on an idle GPU: test 1 stress 3.4-3.8M slot-rounds/s, normal 12-16M/s; test 2 52-59K rounds/s):

| runs | rounds | expected |
|---|---:|---:|
| t1_A_stress | 10^9 | about 280 s |
| t1_A (normal) | 10^9 | about 65 s |
| t1_A_long / t1_C_long | 10^8 each | about 8 s each |
| t1_C_stress / t1_C | 2 x 10^8 each | about 60 s / 15 s |
| t1_Am_stress / t1_Am (calibration) | 10^7 each | about 3 s / 1 s |
| t1_Cd_stress / t1_Cd (calibrated delay) | 2 x 10^8 each | about 55 s / 15 s |
| t1_Cd15_stress / t1_Cd15 (1.5 us, informational) | 10^7 each | about 4 s / 1 s |
| t1_A0_stress / t1_C0_stress / t1_Am0mix_stress (lane 0 idle) | 2 x 10^8 / 10^7 / 10^7 | about 55 s / 4 s / 3 s |
| B, D1, D2 | stress 2 x 10^8 and normal 10^8 each | about 190 s in all |
| E (stress, normal) | 10^7 each | under 10 s each, plus up to 50 s of re-reads |
| test 2 controls | 10^6 each | about 20 s each |
| t2_cas, t2_casfence | 10^7 each | about 170-190 s each |
| t2_casacq, t2_cas_stress | 3 x 10^6 each | about 60 s each |
| **total** | | **about 22 minutes** (quick: about 3 minutes; smoke profile: about 15 s of runs, 30 s with start-up) |

- The main variant A gets 10^9 rounds in the stress and normal modes, ten times the 10^8 the note asks for, and 10^8 in the long mode. Each call stays below 10 minutes.
- Test 2 gets the asked 10^7 for cas and casfence (about 3 minutes each).

## Smoke results (not a measurement)

`results/run-20261115-040157-smoke/` holds the `./run_all.sh smoke` profile run on an idle GPU (load 0), 1-2 x 10^6 rounds per test-1 run:

| mode | A | C | Cd | E |
|---|---|---|---|---|
| stress, chunk 4 | 0 of 2.0M | 2,110 of 1.0M | 645 of 1.0M | violations, all healed within 1 ms |
| normal, chunk 4 | 0 of 2.0M | 295 of 1.0M | 0 of 1.0M | violations, all healed |
| long (1 s launches) | 0 of 2.0M | 3 of 2.0M, all at launch starts | - | - |

- B, D1 and D2 had 0 violations in every mode.
- Test 2: 0 stale and 0 double winners in 5 x 10^4 to 10^5 rounds per variant. The control had stale rounds (cas 6,252 of 20,032; casfence 5).
- Single smokes made while developing (output not kept): stress chunk 4, A 0 and C 11,207 of 4 x 10^6, Cd 3,149 of 4 x 10^6; stress chunk 64, A 0 and C 1,336 of 10^7; normal chunk 4, Cd 0 of 8 x 10^6; long mode with 1 block (8 warps), C 0 of 3 x 10^6.

These are smoke counts, not the result.

### Re-smoke after the review (not a measurement)

`results/run-20261115-044555-smoke/` (`./run_all.sh smoke`, GPU load 0):
- A's fence (Am): median 29.1 us (stress) and 6.7 us (normal) in a launch's first round; 26.6 us and 5.8 us in the other rounds; p99 under 42 us.
- Calibrated Cd: 0 of 10^6 in both modes (C: 1,345 stress, 166 normal), so "the fence's duration". Cd at 1.5 us: 224 of 10^6 (stress), 0 of 10^6 (normal).
- Lane 0 idle (stress): A0 0 of 10^6, C0 1,311 of 10^6. Am0mix: the leader's fence in A0's layout took a median 19.2 us in a launch's first round, a fence in a warp without payload 19.8 us (ratio 1.03), so the verdict is "not separable from the fence's duration". Single smokes in `results/review-smokes-20261115/` (`mix_*.json`) agree: 1.03 (stress, lane 0 idle), 1.03 (stress, lane 0 writing), 1.01 (normal). Under this load a `membar.sys` lasts about as long whatever its own warp wrote.
- A separate single smoke (2 x 10^6 rounds, normal) of Cd at 7.3 us had 1 violation (healed), outside a launch's first round: the rule's ">= 3 failures" floor is there for such isolated events.

## Known limits

- **One process.** A second mapping stands in for the loader process. A separate process would see the same physical pages through its own mapping, but this was not run.
- **Machine, not model.** The test can only show what this GPU, driver and CPU do, never what the model allows (see the reasoning above).
- **Where the reorders were.** In the smokes they appeared almost only in the first round of a launch. **The cause is not established.** Candidates:
  - Effects at kernel start: a GMMU/TLB fill or L2 state on the warp's first access. With S = 4 and 4 rounds per launch, rounds 2-4 also write their slots for the first time in that launch yet rarely fail, so a per-line first touch is unlikely; a per-warp or per-page first access is still possible.
  - Reader timing: readers idle between launches and may catch the flag fastest at launch start. A 1-block long-launch smoke (8 warps for 4 readers, so readers mostly idle) gave C 0 in 3 x 10^6, which argues against reader latency alone, but that is one smoke.
  - Which of these holds also decides how often the race appears in a real tool. Probes write at every kernel exit, so launch starts are frequent there.
- **Time-slicing.** With another GPU process running, a warp can stall for whole slices. That widens race windows and slows rates. The real run must be alone on the GPU.
- **Test 2's plain reads** may hit a stale L1 line only if the line was cached on that SM since the last `CCTL.IVALL`. Every publish executes one, which makes stale L1 hits rare by construction. A design that publishes without a fence would differ.
- **Rates** depend on 4 reader threads, S = 4 and polling. They are not a cost of the protocol. Check 2 (e2-cost) measures the fence costs.
- **Payload sizes.** Only 1-word and 4-word-per-lane payloads were tried, with S = 4 and M = 64.
- **Normal mode reads forward** (lane 0's line first), which gives a late line more time to land than the reverse order of the stress mode. The stress mode and A0 carry the power.
- **Fence duration under load.** With 384 warps publishing at once, a `membar.sys` takes 6-30 us here, so A's rates are bounded by the fence, and the Cd attribution compares against that loaded duration, not the 1.1 us of an idle 64-thread kernel. A fence in a warp with no stores takes as long (Am0mix), so under this load the fence's ordering of other lanes' stores cannot be separated from its duration by these tests. Separating them would need a setting where the fence is short while stores are still in flight, which this harness does not have.

## Changes made in review (2026-11-15, before any real run)

Disclosed because two of them change decision rules after smoke data was seen:
- Review smoke outputs (not measurements): `results/review-smokes-20261115/` (single runs: Am, calibrated and 1.5 us Cd, C, lane-0-idle A/C/Am/E, the mixed fence baseline) and `results/run-20261115-044555-smoke/` (the whole smoke profile).
- **Cd's delay is now calibrated** (Am variant, `run_all.sh` passes the measured median first-round fence duration). The fixed 1.5 us was 5-20 times shorter than A's fence under this load, so the earlier attribution ("Cd > 0: the fence") compared A against a much shorter wait. The attribution rule now also requires calibration and at least 3 Cd failures with an expectation of at least 3 in A's rounds. On the re-smoke this flipped the stress-mode attribution from "the fence" to "the fence's duration".
- **Lane-0-idle runs (A0, C0, Am0mix) and their rule were added**, because A's layout gives little power against a fence that covers only the issuing lane (see Method). The rule grants "orders the stores of lanes that did not fence" only if the fence is measurably longer with stores outstanding in its warp (ratio <= 0.5); in the smokes it was not (ratio 1.03), so the smoke verdict is "not separable from the fence's duration". The A verdict's wording now says what it covers.
- Test 2: a CPU-side winners != 1 is reported as an acquire failure, no longer as a double winner.
- The informational `t1_Cd15*` runs keep the old 1.5 us delay for comparison.


## Files

- `src/pub4.cu`: kernels and host.
- `../common/step0_regmem.h`: registered memory.
- `sass_check.py`: the per-variant fence and store checks.
- `analyze.py`: verdicts.
- `build.sh`, `run_all.sh`: build and run.
- `build/`: `pub4`, `pub4.ptx`, `keep/`, `pub4.sass`, `sass_check.json`.
- `results/`: smoke output only.

# Step 0, check 1: one full-occupancy id log

Design note: `/home/jetson/src/results/gpu/dir1-design-space-20261115/README.md`, section 5, check 1.
Standalone CUDA (no bpftime). Nothing here edits the dev tree.

## The question (quoted from the design note, section 5)

> 1. **One full-occupancy id log** (A, B, C, D).
>    - Each warp stores `%smid`, `%warpid`, `%laneid`, `%nsmid`, `%nwarpid`, `%gridid`, ctaid and globaltimer at entry and at exit, into device memory.
>    - Runs: block sizes 32-1,024; two streams; a 3-node graph replayed 1,000 times; two modules; a second GPU process forcing time-slicing.
>    - Decides:
>      - S and W (A, C, D);
>      - whether (SM, warp slot) is exclusive among resident warps (D's scratch and plain-add counters stand or fall on it);
>      - whether ids change between entry and exit;
>      - whether `%gridid` is unique per launch, replays included (B's per-launch regions stand or fall on it; a launch id for A, C, D).

It also decides whether `%warpid` is CTA-relative (always tid/32) or a hardware slot (section 3, "What `%warpid` is").

## Files

| file | what |
|---|---|
| `rec.cuh` | the 80-byte record and `read_ids()`: every register read with inline PTX |
| `ids.cu` | cases a-e (runtime API; driver API for case d) |
| `idmod.cu` | the same recording kernel for the second and third module (case d) |
| `hog.cu` | the second GPU process for case e |
| `run_e.sh` | case e alone (`ctl`) or with the hog (`hog`) |
| `run_all.sh` | build, SASS check, all cases, analysis, `/dev/shm` check |
| `analyze.py` | per-case statistics and the decisions; `--selftest` runs `decide()` on a synthetic run that must give "yes" for exclusivity, `%gridid` and preemption, then on one defect at a time (collision, duplicate gridid, missing record, missing replay, id change, too few gaps, noisy control, unattributed gaps, missed stall, uncapped control) that must give "no" or "inconclusive" |
| `check_sass.py` | confirms the register reads, spin loads and atomics survive compilation |
| `smoke/` | development runs on the shared GPU (see the end); not results |

## Method

Record: lane 0 of every warp (for block size 48, lane 0 of the second warp is tid 32) writes `%gridid` (read as
`mov.u64` and again as `mov.u32`), `%smid`, `%warpid`, `%laneid`, `%nsmid`, `%nwarpid`, `%ctaid.xyz`, tid/32,
`%globaltimer`, a host tag and a kind into device memory. Records start as 0xFF bytes, so a record nobody wrote is visible.

**a. Co-residency at every block size.** Block sizes 32, 48, 64, 96, 128, 256, 512 and 1,024, three repetitions each.
Each launch has exactly `cudaOccupancyMaxActiveBlocksPerMultiprocessor x 8` blocks. Each warp records its entry ids,
adds 1 to a global arrival counter, waits until the counter reaches the launch's warp count, then records its exit ids.
- The wait is a volatile load loop with two caps: 2^23 iterations (about 3.6 s at the rate seen in the smoke) and
  500 ms of `%globaltimer`. Each cap hit is counted.
- A launch is **proven co-resident** only if no warp hit a cap and every record was written. Warps that passed the
  barrier were then resident at the same time.
- Control that must fail: one launch with twice the resident capacity (block size 128). Its first wave can never
  see all warps arrive, so it must hit the cap; such a launch is never counted as proven.

**b. Two grids at once.** K1 on stream 1 and K2 on stream 2, both non-blocking, same block size (128, 256 or 96;
three repetitions each), each with half of the resident capacity. Both wait on ONE arrival counter covering the warps of
both grids, so a pass proves co-residency across grids. The analysis counts the SMs that held warps of both grids.

**c. `%gridid` uniqueness.** In one process (one context):
- c1: 1,000 launches (2 blocks x 64 threads) on one stream;
- c2: 1,000 launches alternating between two streams;
- c3: a 3-node chain captured from a stream and replayed 1,000 times. Each node takes a replay number from a device
  atomic and records `%gridid` with it;
- c4: a second instance of that graph, 500 replays, `cudaGraphExecUpdate` to a re-captured graph with other kernel
  arguments (as llama.cpp does), 500 more replays.
Issue order is the tag; the analysis checks one gridid per launch, duplicates across launches, and monotonicity in
issue order. Every record buffer is filled with 0xFF and the device synchronized before the launches (`cudaMemset`
runs on the legacy stream, which does not order against the non-blocking streams).

**d. Two more modules.** The recording kernel from `idmod.cu`, loaded twice: as PTX with `cuModuleLoadData`
(driver JIT) and as a cubin with `cuLibraryLoadData` (`cuLibraryGetKernel`, `cuKernelGetFunction`). 300 rounds of
runtime-API launch, PTX-module launch, library launch, all on one stream in one process. Same gridid checks; `%nwarpid`
is also read from the JIT module.

**e. Preemption.** One full-occupancy kernel (96 blocks x 128 threads = 384 warps; launch bounds force the register
budget). Lane 0 of every warp:
1. records its entry ids, passes an arrival barrier (proves co-residency at the start), records again;
2. runs a loop for 300 ms of `%globaltimer`. Each iteration reads `%globaltimer`, `%smid` and `%warpid`, compares
   them with the previous iteration, then sleeps 1 us (`__nanosleep`). A **gap** is a time step > 50 us. An **id change**
   is a different (`%smid`, `%warpid`) from one iteration to the next; changes are counted with and without a gap,
   in counters that do not depend on the event log. Gap and change events go to a log of 1,024 per warp (overflow
   counted), and one sample every 1 ms goes to a second log;
3. passes a second arrival barrier (co-residency at the end) and records its exit ids.
- Iteration cap 2^24 (16.7 s at 1 us per iteration) on top of the 300 ms bound; barrier caps 2^23 iterations and 2 s;
  the injected stall is capped at 2^20 sleeps and the intruder kernels at 2^22. Every cap hit is counted: per warp in
  `e_warps.csv` (`flags` bit 0 loop, 1 entry barrier, 2 exit barrier, 3 stall) and per intruder kernel in
  `e_intruders.csv` (`capped_warps`); the summary reports `warps_with_cap_flags`.
- **Detector control (must be detected):** at 150 ms every warp stalls itself for 200 us (sleeping, so it does not
  starve the others). The next iteration must see a gap; it is counted separately as "injected", not as preemption.
- Runs: `e_ctl` (alone; the gap threshold is valid only if its largest time step is < 50 us), `e_hog` (while
  `hog` runs in a second process: 96 x 128-thread kernels of 50 ms back to back for 6 s; its warp 0 logs its own
  gaps, so the analysis can check that our gaps fall where the hog was running), and `e_prio` (extra: ten
  8-block kernels on a high-priority stream of the same process while the long kernel runs on the low-priority stream;
  this tests whether same-context priority preemption happens at all).
- An **episode** is a set of overlapping gap intervals (merged over warps) that touches at least half the warps.

Why the 1 us sleep: in the first smoke, without it, the warp scheduler served the oldest warp slots first. Slots 0-3
ran 923k iterations in 300 ms and slots 36-39 ran 1.5k, so the youngest warps saw "gaps" of about 190 us with no
preemption at all. With the sleep, the control run shows 0 gaps.

## What the compiler makes of each register (sm_87, nvcc 13.2; `check_sass.py`)

| PTX | SASS | consequence |
|---|---|---|
| `mov.u32 %smid` | `S2R SR_VIRTUALSMID` | a virtual SM id |
| `mov.u32 %warpid` | `S2R SR_VIRTID`, then bits 8-14 (`SHF.R.U32.HI 8`, `SGXT.U32 7`) | a 7-bit field: values up to 127 are possible in principle; the analysis never assumes < 48 |
| `mov.u32 %laneid` | `S2R SR_LANEID` | |
| `mov.u32 %nsmid` | load from constant bank `c[0x0][0x10c]` | written by the driver per launch |
| `mov.u32 %nwarpid` | the immediate 0x30 (48) | ptxas's constant for sm_87, not a hardware read |
| `mov.u64 %gridid` | 64-bit load from `c[0x0][0x30]`/`c[0x0][0x34]` | a per-launch constant-bank word written by the driver |
| `mov.u32 %gridid` | the low word `c[0x0][0x30]` only | ptxas accepts both widths, so acceptance does not settle the width; the records compare them |
| `mov.u64 %globaltimer` | one `CS2R Rx, SR_GLOBALTIMERLO` (64-bit pair) | no torn LO/HI read |

`check_sass.py` also requires that `k_long` reads `%smid`, `%warpid` and `%globaltimer` inside the timed loop (between
the back-branch target and the back-branch that encloses the per-iteration `NANOSLEEP`), not hoisted out of it, and it
requires the volatile spin load (`LDG.E.STRONG.SYS`), the arrival atomics, the replay-counter
fetch-add with a used result, and `NANOSLEEP` in the loops. `run_all.sh` stops if any requirement fails.

## Decision rules (fixed before any real run; thresholds are constants at the top of `analyze.py`)

Amended in review (2026-11-15, still before any real run; the code was out of line with the text): post-entry-barrier
collisions in e, a/b entry-to-exit changes in the preemption "no", hog attribution as a condition for "yes", record
completeness as a condition for the `%gridid` "yes", and "not tested" for the priority extra.

**S and W.** S = max(`%smid`)+1 over the records of a, b and e, and the `%nsmid` values; W = max(`%warpid`)+1 over
the same records, and the `%nwarpid` values (d's JIT module included). Both are reported, not judged. A max `%warpid`+1 above `%nwarpid` would be reported as such.

**(smid, warpid) exclusive among co-resident warps.**
- **no**: any collision (two warps with the same pair) among the entry or the exit records of any proven launch
  in a or b, among the post-entry-barrier records of an e launch whose entry barrier passed, or among the exit
  records of an e launch whose exit barrier passed.
- **yes**: 0 collisions in every proven launch, AND every regular launch of a and b is proven, AND the oversubscribed
  control hit the cap, AND in at least one proven launch with >= 2 CTAs on one SM the control key (smid, tid/32)
  collides (the counter can count), AND at least one proven b pair had both grids on the same SM.
- **inconclusive**: otherwise (for example a launch not proven, or a control that did not fail).

**%warpid kind.** CTA-relative if `%warpid` == tid/32 in every warp; hardware slot if it differs in some warps and
exclusivity is "yes"; otherwise unclear.

**ids change between entry and exit.** Count of warps whose (smid, warpid) differs between entry and exit, in a and b
(and between post-barrier and exit in e). Reported; any change also makes the preemption answer "no".

**ids stable after preemption.**
- Preconditions: in every e run each warp detected its injected stall (detector works), and the control's largest
  non-injected time step is < 50 us (threshold valid; this fails on a busy GPU).
- Attribution: in every hog launch `gap_time_covered_by_hog_running` (share of our gap time during which the hog's
  warp 0 was running) is >= 0.5, and `hog_alive_at_end` (from `e_hog/ids_e.log`) is true. Otherwise the gaps are
  not shown to be the hog's time slices.
- **no**: any id change in any e run (with or without a gap, or in the 1 ms samples, or entry-to-exit), or any
  entry-to-exit change in a or b.
- **yes**: 0 changes, preconditions met, attribution met, >= 10 episodes in the hog run, >= 1,000 warp-gaps in the
  hog run.
- **inconclusive**: otherwise.
- Changes without a gap are reported separately.

**%gridid unique per launch.**
- **no**: any duplicate across launches in c1, c2, c3, c4 or d.
- **yes**: 0 duplicates in all five parts, every graph node's replay counter equals the replays requested, no
  unwritten record in c1, c2 and d, c1/c2 with the requested launch count and c3/c4 with 3 nodes x replays, no graph
  node overflow, and every launch of a and b has exactly one gridid. (A dropped launch could hide a duplicate.)
- **inconclusive**: otherwise.
- Monotonicity (strictly increasing in issue order) is reported per part. Width: the number of records where the
  `mov.u32` value differs from the low half of the `mov.u64` value, and where the high half is non-zero.
- Values are never compared across processes (the PTX ISA calls `%gridid` a per-context number).

**Same-context priority preemption (extra).** "yes" if any high-priority kernel started inside the long kernel's
loop window (GPU clock); "no" only if at least one intruder was issued while the long kernel ran (host clock:
`host_issue_ms` < the 300 ms loop length, counted from the long kernel's launch call) and every intruder left its
timestamps; "not tested" otherwise. Its id changes count toward the preemption answer.

## How to run (main session, quiet board)

```
cd /home/jetson/src/bpftime-gpu-work/step0/e1-ids
./run_all.sh            # -> runs/<date-time>/ ; or ./run_all.sh <OUT>
```

- Expected wall time: about 30 s (a 2 s, b 1 s, c 1 s, d 1 s, e_ctl 3 s, e_hog 8 s, e_prio 3 s, build and analysis
  the rest). Every GPU step has its own `timeout -k 5` (60/30/30/30/70/100/70 s); every device wait has a cap.
- Output: `OUT/summary.txt` and `OUT/summary.json` (decisions plus per-launch details), raw CSVs per case in
  `OUT/{a,b,c,d,e_ctl,e_hog,e_prio}/`, `OUT/check_sass.txt`, the SASS and PTX, `OUT/env.txt`, `/dev/shm` before/after.
- Nothing creates POSIX shm. CUDA can leave `/dev/shm/cuda.shm.<uid hex>.<pid hex>.*` behind when a process is
  killed; `run_all.sh` removes such entries only for PIDs it started (from the meta files and `hog.log`).
- Single parts: `./build/ids <a|b|c|d|e> <OUTDIR> [key=value]` (keys: reps, cap_ms, cap_iters, n, dur_ms, gap_us,
  period_us, inj_us, sleep_ns, evcap, smpcap, prio, intr_ms, intr_gap_ms, intr_blocks); `./run_e.sh OUT hog|ctl REPS HOG_S KERNEL_MS [key=value]`.
- `python3 -I analyze.py --selftest` must print `selftest ok`.

## Known limits

- Preemption is exercised only as time-slicing between two contexts with full-occupancy kernels (the hog) and as
  same-context stream priority. Not exercised: the debugger (cuda-gdb), CUDA checkpoint/restore, compute-sanitizer,
  power gating or reset. A context switch may save and restore a whole SM image, which would keep ids by
  construction; "stable" means stable under these two mechanisms only.
- `%smid` is `SR_VIRTUALSMID`. On this board it equals 0-7; a partitioned GPU could differ.
- `%nwarpid` is the ptxas constant 48, not a hardware read; W rests on the observed maximum.
- Co-residency and exclusivity are tested within one context. Across processes the Orin time-slices, so two contexts'
  warps are never resident together.
- Only lane 0 runs the barriers and loops; lanes 1-31 wait at `__syncwarp`. Warps are recorded, not threads.
- Graphs: a stream-captured 3-kernel chain, plus `cudaGraphExecUpdate` with new kernel arguments. Not tested: graphs
  with parallel branches, child graphs, device-side graph launch, conditional nodes, and graphs on several streams.
- `%gridid` advances on every kernel the context runs, including the runtime's own (in a, launches 2 apart because of
  the `cudaMemset` between them). Only uniqueness and order are judged, not the step.
- Case d loads the modules through the driver API directly, not through bpftime's `cuLibraryLoadData` hook.
- The wall times in the CSVs are not measurements (the GPU was shared during development).

## Smoke runs (development, 2026-11-15; not results)

`smoke/run3` (the full `run_all.sh` after review, 14 s; no other GPU process was running at that moment, but the
board was not prepared: other agents were building on the CPU) printed:
- S = 8 (`%nsmid` 8); W = 48 (`%nwarpid` 48);
- exclusive: yes. 0 collisions in 33 proven launches; the oversubscribed control capped; the (smid, tid/32) key
  collided; all 9 b pairs had both grids on every SM. `%warpid` equals tid/32 in only 466 of 10,752 warps: a
  hardware slot;
- `%gridid`: "no". 0 duplicates and strictly increasing in c1, c2 and d, with 0 missing records. Each graph node kept
  one gridid over all 1,000 replays, also across `cudaGraphExecUpdate` (2,997 duplicates in c3 and in c4). The three
  nodes of one graph had different gridids, and the two graphs had disjoint ones. The u32 read equals the low half
  of the u64 read; the high half was always 0. The "yes" path of this rule was reached only in `--selftest`;
- ids stable after preemption: "yes". The control had 0 gaps (max step 8 us). The hog run had 198 episodes and
  76,032 warp-gaps, with hog coverage 0.81 in every launch. There were 0 id changes, 0 cap hits, and every warp
  detected its injected stall;
- e_prio: "no". All 30 intruders were issued 20-200 ms into the 300 ms loop, and every one started after it ended.
`smoke/run2` is the writer's run before review. It ran on a GPU other agents' processes were using. Its e verdict was
"inconclusive" because the control had gaps and hog coverage was 0.43. In it, e_prio would now read "not tested", since
it predates the intruder issue times.
These are smoke outputs only; the main session's run on a prepared quiet board decides.

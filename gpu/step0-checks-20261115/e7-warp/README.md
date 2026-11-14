# Step 0, check 7 (standalone half): warp instructions in a `__noinline__` helper under partial masks

Design note: `/home/jetson/src/results/gpu/dir1-design-space-20261115/README.md`, section 5, check 7.
Standalone CUDA (no bpftime). The other half of check 7 (`popc(activemask)` at the exit probe of 4 llama.cpp
kernels) is not here.

## The question (quoted from the design note, section 5)

> 7. **Warp instructions in a `__noinline__` helper under partial masks** (C, D).
>    - `shfl.sync`, `bar.warp.sync` and `activemask` with early returns that depend on data, and inside `__memcapture`.
>    - `popc(activemask)` at the exit probe of 4 llama.cpp kernels.
>    - Decides whether warp-level reservation is correct, and the records per call (C's chunk size and its waste).

This directory answers the first bullet and the first half of "Decides". Records per call here come from synthetic
kernels; the llama.cpp exit-probe half gives the real distribution.

## Files

| file | what |
|---|---|
| `warp.cu` | the helpers, five kernels (templates over the helper variant), the host-side checker |
| `check_sass.py` | per kernel: the helper is a real `CALL.REL.NOINC` target and keeps its warp instructions; writes the helper bodies as evidence |
| `analyze.py` | totals, histograms, chunk waste, decisions; `--selftest` |
| `run_all.sh` | build, SASS checks, native and JIT runs, analysis, `/dev/shm` check |
| `smoke/` | development runs on the shared GPU; not results |

## Method

**The helper under test** (`reserve_ok`, `extern "C" __device__ __noinline__`, as bpftime declares its helpers):
`mask = __activemask()`; `leader = __ffs(mask) - 1`; `rank = __popc(mask & %lanemask_lt)`; the leader does
`base = atomicAdd(cursor, __popc(mask))`; `base = __shfl_sync(mask, base, leader)`; `__syncwarp(mask)`;
return `base + rank` (packed with `popc(mask)`, a leader flag and the leader lane, all taken from the true mask).

**Ground truth, kept by the caller outside the helper.** Each calling thread writes the tag (thread id << 32 | its
own call number) at the returned index, adds 1 to a per-index write counter, and counts its own calls. Every return
path stores the thread's count first (counts start as 0xFFFFFFFF, so a missing store is caught). The leader lane adds 1
to a histogram by popc(mask) and by leader lane, per call site. The host checks every launch:
- `count_mismatch`: |cursor - sum of the threads' counts|;
- `missing` / `double`: indices in [0, total) written 0 times / more than once;
- `oob`: an index >= capacity (never written, counted on the device);
- `bad_tag`, `lost_call`, `dup_tag`: a tag that names no real call, a real call whose tag is nowhere, a tag twice;
- `hist_mismatch`: sum of popc x histogram != calls.
`violations` is their sum and must be 0.

**Which call sites share one call (caller side, helper unchanged).** Each lane also stores its call site at its
returned index, and the leader stores `popc(mask)` at its own index, which for the correct helper is the reservation
base. For the correct helper the host walks the reservations `[base, base + n)`; they must tile `[0, total)` (tiling
errors are counted; if any, the share below is reported as unavailable). Each reservation is classified by the number
of distinct call sites among its lanes (`sites.csv`; `mixed_site_reservations` in `results.csv`). This is reported,
not judged. It was added in review because the per-site means in the first smokes were equal across sites whose
branch probabilities differ by 2x, while the SASS has a separate `CALL` per site: lanes reaching the shared helper
from different call sites run it together.

**Kernels** (128 threads per block, `__launch_bounds__(128, 12)`, 96 blocks = full occupancy on 8 SMs; per-lane
predicates from a host-generated input array, a new seed per launch, case and variant; launches repeat until the call target):
- `early`: about 1 in 16 lanes returns from the kernel per iteration; about 3 in 4 of the rest call. 64 iterations.
- `lowlanes`: lane 0 and the lowest 1-31 lanes (a per-warp cut taken from the data) return before any call; more
  lanes return later; two call sites in an if/else. 64 iterations.
- `nested`: nested divergent branches with three call sites (two inside the inner if/else). 64 iterations.
- `loops`: a call inside a loop whose trip count depends on the lane and the data (0-8), with a data-dependent break.
  32 iterations.
- `memcap`: a call immediately before each global load and store of a data-dependent gather/scatter, including a store
  under divergence and loads inside a data-dependent loop (bpftime's `__memcapture` calls before every ld/st).
  32 iterations.

**Variants.** `ok` (the design), and two wrong helpers that the checker must catch:
- `bad_rank`: rank from the full mask (rank = lane). No undefined operation; with absent lanes it leaves holes and
  overlaps other warps' ranges.
- `bad_leader0`: leader fixed to lane 0. The shuffle still uses the active mask (no hang); with lane 0 absent nobody
  reserves and the shuffled value is undefined.
No full-mask `__shfl_sync` with absent lanes is used anywhere.

**Two build paths, both run.**
- `native`: the nvcc-built binary (the calls are `call.uni` in PTX, `CALL.REL.NOINC` in SASS).
- `jit`: the same PTX with every `call.uni` turned into a plain `call` (as bpftime inserts them), loaded with
  `cuModuleLoadData` (driver JIT, as bpftime loads patched modules) and launched with `cuLaunchKernel`.
  The JIT's SASS cannot be dumped, so the same PTX is also assembled offline with ptxas for `check_sass.py`.
  In development that offline SASS was identical to the native SASS.

**SASS evidence** (`check_sass.py`, `OUT/helpers_*.sass.txt`): in every one of the 15 kernels the helper is reached by
`CALL.REL.NOINC` from at least as many sites as the source has (memcap: 10 sites in SASS for 4 in source, from
unrolling), and its body holds `VOTE.ANY` (activemask), `FLO` (lowest lane), `POPC`, `ATOMG.E.ADD` (the leader's
fetch-add with the result used), and `SHFL.IDX` and `WARPSYNC`. nvcc emits the shuffle and `__syncwarp` as a fast path
behind `BRA.CONV` (taken when the mask is the converged warp) plus a slow-path subroutine with `WARPSYNC`; the check
follows those subroutine calls. Observation: nvcc compiles the helper once per kernel, so its body differs between
kernels and between builds in register numbers and in one or two register moves (the warp instructions, atomics and
convergence-barrier instructions have the same counts); `helpers_*.sass.txt` is therefore not byte-stable.

**Records per call.** From the histogram: mean, median, share of calls with popc 1 and popc 32, the share of calls
with lane 0 absent, the waste of approach C for chunk sizes C = 1, 2, 4, 8, 16, 32 (a call claims ceil(n / C)
chunks; waste = 1 - records / slots claimed), and the share of calls whose lanes came from more than one call site.
The histogram is filed under the LEADER's call site (`records_per_call_mean_by_leader_site`): the other lanes of the
same call may come from other sites.

## Decision rules (fixed before any real run; constants at the top of `analyze.py`)

Per build path (native, jit):
- **no**: any violation with the correct helper in any case.
- **yes**: 0 violations, AND every case ran with >= 1,000,000 calls, AND every case had calls with popc < 32, AND every
  case had calls with lane 0 absent, AND in `lowlanes` lane 0 was absent in every call, AND `bad_rank` produced
  violations in every case (the checker can fail), AND `check_sass.py` passed for that path.
- **inconclusive**: otherwise; the reasons are listed.
Overall: "no" if either path says no, "yes" if both say yes, else inconclusive. `bad_leader0` violations are reported;
they are expected at least in `lowlanes` but do not enter the rule.

## How to run (main session)

```
cd /home/jetson/src/bpftime-gpu-work/step0/e7-warp
./run_all.sh            # -> runs/<date-time>/ ; or ./run_all.sh <OUT>
```

- Expected wall time: under 10 s (the development run took 1.5 s plus the build). Two GPU steps, each under
  `timeout -k 5 60`. This is a correctness test: a busy GPU changes timing, not the verdict.
- Correct helper: >= 4,000,000 calls per case; each control: >= 200,000 calls per case.
- Output: `OUT/summary.txt`, `OUT/summary.json`, `OUT/{native,jit}/{results,hist,leader,sites}.csv` (one row per
  launch; histograms per case, variant and call site; reservations by number of distinct call sites), `OUT/check_sass_{native,jit}.txt`, `OUT/helpers_*.sass.txt`,
  `OUT/sass_native_vs_plaincall.diff` (empty when the offline-assembled plain-call PTX gives the same instructions
  as the native build).
- Single runs: `./build/warp <case|all> <ok|bad_rank|bad_leader0|all> <OUTDIR> [min_calls= ctl_min_calls=
  max_launches= seed= blocks= iters= ptx=<file>]`. For a first try on a new build, use `blocks=1 iters=4`.
- `python3 -I analyze.py --selftest` must print `selftest ok`.

## Known limits

- The helper is CUDA C++ compiled by nvcc and called from CUDA C++. bpftime's helpers come from clang
  (`default_trampoline.cu`) and its probes from eBPF via LLVM, and the calls are inserted into the application's PTX.
  The `jit` path covers the driver JIT and the plain `call`, not clang's code for the helper or a call inserted into
  foreign PTX. CALL in SASS is shown only for these binaries.
- The kernels are synthetic. Their records-per-call distribution says how many lanes run one helper call together in
  these patterns (lanes from different call sites included), not in llama.cpp (that is the other half of check 7).
- The helper reserves one fixed-size unit per lane, and each lane writes its own unit. Not exercised: a leader that
  writes per-reservation metadata (a chunk header, a site or probe id), and call sites with different record sizes.
  Since one call can hold lanes from several call sites, such metadata could not assume one site per call.
- One cursor for the whole GPU. Contention is real (all warps), but the per-SM claim counter and the CAS from free to
  writing of approach C, and D's CAS loop on a ring's reserve word, are not exercised.
- The checker sees indices below the capacity only; writes past it are counted (`oob`) but not inspected.
- `bad_leader0` reads an undefined shuffle value by design; its violation counts vary between runs.

## Smoke runs (development on the shared GPU, 2026-11-15; not results)

`smoke/run3` (the full `run_all.sh` after review, about 2 s of GPU work) printed "correct under partial masks: yes"
for both paths:
- correct helper: 0 violations over 4.00-4.31M calls per case at 12 blocks/SM; reservations tiled `[0, total)` with
  0 errors;
- `bad_rank`: 219k-438k violations per case; `bad_leader0`: 499k-1.04M;
- mean records per call: early 6.7, lowlanes 5.7, nested 12.5, loops 17.4, memcap 7.6; popc 32 in at most 1.6% of
  calls; lane 0 absent in 61-79% of calls (100% in lowlanes); chunk waste 0.18-0.43 at C = 8, 0.46-0.82 at C = 32;
- calls holding lanes from more than one call site: lowlanes 68%, nested 99.6%, memcap 73% (the cases with one
  site: 0%). In memcap, lanes about to load and lanes about to store share one reservation;
- the offline-assembled plain-call PTX gave the same instructions as the native build (`sass_native_vs_plaincall.diff`
  empty).
`smoke/run2` is the writer's run before review (no call-site data). These are smoke outputs only; the main session's
run decides.

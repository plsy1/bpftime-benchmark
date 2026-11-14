# e7-llama: active lanes per exit-probe call on four llama.cpp kernels (step 0, check 7, llama part)

Design note: `/home/jetson/src/results/gpu/dir1-design-space-20261115/README.md`, section 5, check 7:

> 7. **Warp instructions in a `__noinline__` helper under partial masks** (C, D).
>    - `shfl.sync`, `bar.warp.sync` and `activemask` with early returns that depend on data, and inside `__memcapture`.
>    - `popc(activemask)` at the exit probe of 4 llama.cpp kernels.
>    - Decides whether warp-level reservation is correct, and the records per call (C's chunk size and its waste).

This directory covers the second bullet only (the first is `../e7-warp`). It answers: when one lane per
exit-probe call reserves space for the whole call (C's chunk claim, D's slot claim), how many records does a call
carry on real kernels, and how many reserved slots stay unused for chunk sizes C = 1..32?


## Method

- **Instrumentation build** (section at the end): helper 520 returns `activemask` (u64). Nothing else in the
  probe path differs from dev 52126f33.
- **Probe** (`e7.bpf.c`, one source, one object per kernel): at every exit-probe call each lane reads
  `m = helper 520` and `lane = helper 511` first. Only the lowest active lane (`m & -m == 1 << lane`) touches
  the map: one fetch-add of 1 into bin `popc(m)` of a 1513 array (host-registered, 8 copies by `smid % 8` to
  spread the adds; `popc` with SWAR bit tricks, unit-tested in `popc_test.c`). Thread (0,0,0) of block (0,0,0)
  also counts the launch. Lanes whose own bit is missing from `m`, or an empty `m`, count as anomalies.
  The atomic is the unused-result form: BPF `lock *(u64 *)(r0 + 0) += r1` (`bpf/*.dis`), PTX `atom.add.u64`.
- **Loader** (`e7_loader OBJ OUT.json`, plain libbpf, no skeleton): attaches every program, prints a line per
  second, and at SIGINT reads the whole map and writes the raw words and a per-kernel summary.
- **Kernels** (the SEC names of `example/gpu/{threadhist-llama,th-mmq,th-mmvq,th-ropek}`):
  rms = `rms_norm_f32<512,1,true,true,false,false>`, mmq = the largest `mul_mat_q` (40.6% of pp512 GPU time),
  mmvq = the largest `mul_mat_vec_q` (64.7% of tg128 GPU time), ropek = `rope_norm<true,false,float,half>`
  (RoPE K). `mul_mat_q` does not run in decode (census), so tg128 probes rms, mmvq and ropek.
- **Runs** (`run_all.sh`, all through `bpftime-gpu-work/run.sh` with `GPU_BUILD` = the instrumentation build,
  `SPDLOG_LEVEL=info`, `GGML_CUDA_DISABLE_GRAPHS` unset = graphs on): llama-bench `-r 1 --no-warmup`,
  pp512 (`-p 512 -n 0`) x {rms, mmq, mmvq, ropek} and tg128 (`-p 0 -n 128`) x {rms, mmvq, ropek}: one object
  and one run per kernel (7 runs). `ALL4=1` adds one pp512 and one tg128 run with all four programs in one
  object (exploratory; multi-program attach works: the control object carries three programs).
- **Controls** (`e7_ctrl.cu`, one victim, one object `bpf/e7_ctrl.bpf.o` with three programs; PTX in
  `build-ctrl/`; 10 launches x 64 blocks each):
  - `e7ctrl_partial` (pass/fail): blocks of 48 threads, no branch, one `ret`: warp 0 has 32 lanes, warp 1
    has 16. Must give exactly 640 calls of 32 lanes, 640 of 16, 10 launches, 30,720 threads. A helper 520
    that returned a constant full mask would give 1,280 calls of 32 lanes and 40,960 threads.
  - `e7ctrl_diverge` (pass/fail): blocks of 64 threads. Odd lanes leave through their own `ret` (inline PTX);
    even lanes first wait ~200 us (globaltimer + nanosleep, iteration cap, cap hits counted and printed) and
    then leave through the final `ret`. Two `ret`s in PTX, so two probe call sites. Must give exactly 2,560
    calls of 16 lanes (two per warp, one led by lane 1), 10 launches, 40,960 threads. A helper that ignored
    divergence (full mask, or the mask at kernel entry) would give 1,280 calls of 32 and the same thread
    total: the thread-total check alone cannot see that, this control can.
  - `e7ctrl_merge` (reported, not pass/fail): the same two exits without the wait. It measures whether two
    exit groups that reach the probe close together merge inside it (see `merge-test/`).
  - The victim checks its own output (odd lanes add 1, even lanes add 2), so the early `ret` is real.
- **Compiled code** (`check_ptx.py`, once per control kernel): the control run keeps its patched PTX
  (`BPFTIME_CUDA_KEEP_PTX=1`; the directories the agent names in its log are moved into the run directory).
  - PTX: the probe call before every `ret` (1 for partial, 2 for diverge and merge), calls to helpers 520,
    511, 509 and 1, an `atom/red .add` on 64 bits, `activemask.b32` in helper 520.
  - SASS (`ptxas -arch=sm_87`, `cuobjdump -sass`), scoped by address because ptxas puts the whole trampoline
    into the kernel's section and other helpers carry their own `VOTE` and `ATOM`: the probe is the
    subroutine the kernel body calls; its first call must reach a subroutine with `VOTE(U).ANY` (helper 520),
    some call must reach `SR_LANEID` (511), and the probe's own range must hold the `ATOM/RED .ADD`.
    For `e7ctrl_diverge` and `e7ctrl_merge` the body must call the probe from 2 separate sites.
  - The llama runs use the same probe body with another SEC name; their modules' PTX is not kept (size).
- **Analysis** (`analyze.py RESULTS_DIR`): per (label, test, kernel) the histogram of lanes per call, mean
  records per call, the checks below, and for C = 1..32: unused slots per call (`ceil(k/C)*C - k` averaged
  over calls), the unused share of reserved slots and chunks per call. Outputs `summary.txt`, `summary.json`,
  `hist.csv`, `waste.csv`. Census numbers come from `bpftime-gpu-work/llama/census/{pp512,tg128}-graph.sqlite`
  (nsys, same llama-bench arguments), with the census README's table as fallback.

## Decision rules (written 2026-11-15 before any real run; amended by the review the same day, still before any main-session run)

Validity:
1. **Controls pass** iff `e7ctrl_partial` gives exactly {16: 640, 32: 640} and `e7ctrl_diverge` exactly
   {16: 2,560}, each with 10 launches, the expected threads (30,720; 40,960), no anomaly and no failed
   lookup. If either fails or is missing, every result is INCONCLUSIVE and no interpretation is printed.
   (Amended: the divergence control was added because the first control cannot fail when a mask ignores
   divergence; see the smoke results.)
2. **Compiled-code checks pass** (`ptxcheck.check.json` and `ptxcheck-div.check.json` with `"pass": true`,
   PTX and scoped SASS as above). If either failed or is missing: INCONCLUSIVE, no interpretation.
3. **A llama run is valid** iff its launch count equals the census launches and `sum_k k * hist[k]` equals the
   census thread executions exactly, with no anomaly and no failed lookup (pp512: rms 45 / 11,273,216,
   mmq 21 / 1,892,352, mmvq 2 / 983,040, ropek 22 / 11,534,336; tg128: rms 5,760 / 2,949,120,
   mmvq 7,040 / 3,137,339,392, ropek 2,816 / 2,883,584). An invalid run makes that kernel INCONCLUSIVE (the
   probe missed or fired twice, or the run differs from the census). A planned (test, kernel) with no census
   launch is INCONCLUSIVE too (a name that matches nothing would give 0 = 0 and vanish from the verdict),
   except tg128 mul_mat_q in the `ALL4` run, which is reported as not run. `check_targets.py` checks before
   any run that every object's `kretprobe/` name is analyze.py's name and has the census counts above.

Interpretation (valid kernels only, and only when rules 1 and 2 hold):
- **FULL WARPS** (warp-level reservation of 32 slots per call is cheap): at least 99% of calls carry 32 lanes on
  every valid kernel.
- **C\***: the largest chunk size C whose unused share of reserved slots is at most 10% on every valid kernel.
  This is the answer for C's chunk size. With FULL WARPS, C\* = 32.
- **PARTIAL** (32-slot chunks are wasteful): mean records per call below 16 on some kernel. At C = 32 the
  unused share is `1 - mean/32`, so over half the reserved slots stay empty there; C must be at most C\*.
- **MIXED** (neither): some kernel has under 99% full calls but every mean is at least 16; the answer is C\*,
  and the waste table gives the cost of any larger C.
- Mean records per call is also D's slots per ring claim, and A's lane-0 sampling keeps 1 record of each call:
  it drops `1 - calls/threads` of them.
- What the lane counts mean (added by the review): they are the masks a helper *inside the exit probe* sees.
  The probe starts with `YIELD` and a global load of the probe switch, and two exit groups of one warp that
  reach it close together merge there (`e7ctrl_merge`, `merge-test/`). So FULL WARPS says a per-call
  reservation in an exit probe would see whole warps; it does not say the kernel's own exits were converged,
  and the merge is timing-dependent, not guaranteed.

## Commands (for the main session; counts only, no timing, but run it alone)

| what | command | wall |
|---|---|---|
| build objects (done; no GPU) | `make -C /home/jetson/src/bpftime-gpu-work/step0/e7-llama` | 1 min |
| preflight only | `bash preflight.sh` | 5 s |
| analysis smoke, no GPU (done) | `CTRL_RUN=<a control run dir> bash smoke_analysis.sh` | 2 s |
| target check, no GPU (done: pass) | `python3 -I check_targets.py` | 1 s |
| controls only (done: pass, `results/controls-only-1115-050054`) | `CONTROLS_ONLY=1 timeout -k 10 300 bash run_all.sh` | 30 s |
| smoke (done, below) | `timeout -k 10 900 bash smoke_run.sh` (`PARTS="ctrl pp32 tg16 tg8mmvq"`) | ~3 min |
| **full run** | `timeout -k 10 3600 bash run_all.sh` (`ALL4=1`: 2 more runs, `timeout -k 10 4500`) | ~8 min |
| analysis again | `python3 -I analyze.py results/<stamp>` | 1 s |

`run_all.sh` stops before running anything if the preflight fails (build not finished, instrumentation missing,
passes not from this build, leftover llama-bench/loader/cudagraph processes, leftover e7/e9 shm segments, graphs
disabled in the environment, less than 2.5 GiB available) or the target check fails, and stops after the
control run unless both pass/fail controls and both PTX/SASS checks pass (rules 1 and 2). Run directories: `bpftime-gpu-work/runs/step0-e7-<label>-<test>-<stamp>`
(linked from `results/<stamp>/`).

## Smoke results (2026-11-15, board shared with other agents; counts only)

Writer's smoke (04:13-04:25 JST; `smoke/run-1115-041347` control + pp32, `smoke/run-1115-041718` tg16,
`smoke/run-1115-042408` tg8 mmvq):
- rms, pp32 (no graphs in prefill): 45 / 45 launches, 705,536 / 705,536 threads; 22,048 calls, all 32 lanes.
- rms, tg16 with graphs on: 720 / 720 launches, 368,640 / 368,640 threads ([arith] 45 launches of 512 threads
  per token); 11,520 calls, all 32 lanes. The exit probe counts every replay.
- mmvq, tg8 with graphs on (the heaviest probe path): 440 / 440 launches, 196,083,712 / 196,083,712 threads
  ([arith] 55 launches per token); 6,127,616 calls, all 32 lanes, within 3 s after a 38 s start-up (patching
  and compiling `mul_mat_vec_q`), so the full tg128 mmvq run (98M calls) should take about 90 s: that sets the
  300 s victim cap.

Review (04:42-04:47 JST):
- First divergence control without the wait (`smoke/run-1115-044242`, now `e7ctrl_merge`): 1,280 calls of 32
  lanes instead of 2,560 of 16, with two separate probe call sites in SASS and correct victim output. The
  thread total matched, so rules 1-3 of the writer's version passed while the result could not tell a
  divergence-blind mask from merging. `merge-test/` (plain CUDA) shows `activemask` reports each group when
  the groups stay apart, and the whole warp once both enter a function that starts with `YIELD` and a global
  load, which is how bpftime's probe starts.
- With the wait (`smoke/run-1115-044644`): `e7ctrl_partial` exact (640 x 32 + 640 x 16), `e7ctrl_diverge`
  exact (2,560 x 16, 0 cap hits), `e7ctrl_merge` 1,280 x 32 (every warp merged); all three PTX/SASS checks
  pass (probe sites 1, 2, 2; 520 is `VOTEU.ANY`; the histogram add is `ATOM.E.ADD.64.STRONG.GPU`).
- `smoke/recheck-1115/`: this control plus the writer's pp32, tg16 and tg8 llama runs: VALID, FULL WARPS on
  all three (early sign only; mmq and ropek not run, pp512/tg128 not run).
- `smoke_analysis.sh` (fabricated outputs, `smoke/make_fake_e7.py`, 10 cases): mixed, partial and full llama
  cases give MIXED / PARTIAL / FULL WARPS; a divergence-blind 520, a missing divergence control, a missing or
  failed PTX check and a missing control all give INCONCLUSIVE with empty interpretation lists; a planned
  kernel the census never ran gives INCONCLUSIVE for it; all4's tg128 mul_mat_q is reported as not run;
  `check_ptx.py` passes the real control SASS and fails it without helper 520's `VOTEU` or without the
  probe's `ATOM.ADD`.
- `run_all.sh` with `CONTROLS_ONLY=1` (05:00 JST, `results/controls-only-1115-050054`): preflight, target check
  and the control run as above (all exact, all checks pass), stopped before llama; no leftovers, shm unchanged.

## Known limits

- The lane counts are the masks a helper inside the exit probe sees, after any merge at the probe's entry
  (see the interpretation note). Lanes that reach the probe far apart make separate calls.
- Exit probes only. Entry probes run in every thread at kernel start; `__memcapture` is `../e7-warp`'s part.
- The census thread counts come from a separate nsys run with the same arguments; the census README found them
  identical with graphs on and off.
- The PTX/SASS checks run on the control kernels, not on the llama modules (same probe body, different SEC).
- The adds go to host-registered memory with one `membar.sys` per lookup (inside helper 1): slower runs, exact
  counts. Nothing here is a cost figure; a heavier probe body could change when groups meet.
- `run.sh` names each run's segment `bpftime_gpu_<NAME>` (not `step0_`; `run.sh` is shared and was not
  changed) and removes it; the scripts diff `/dev/shm` before and after.
- tg16's and tg8's expected counts are [arith] (per token, from the tg128 census), used by the smoke only.

## Instrumentation build (shared with ../e9-hooks)

- Worktree `/home/jetson/src/step0-bpftime-exp`, local branch `exp/step0-instrumentation`, commit `7e78a5d2`
  on dev 52126f33 ("step0: instrumentation helpers 520-523 and launch geometry log (experiment only)").
  Throwaway: never push.
- Changes (4 files, +444 lines):
  - `attach/nv_attach_impl/trampoline/default_trampoline.cu`: `_bpf_helper_ext_0520` (activemask, u64),
    `0521` (`%gridid`, u64), `0522` (`%nsmid`), `0523` (`%nwarpid`); `trampoline_ptx.h` regenerated with
    clang++-16 (the diff adds only these four functions). 520-523 were unused (grep).
  - `attach/nv_attach_impl/pass/ptxpass_core/src/core.cpp`: registered next to 509-511 (the only helper table
    `generate_ptx` uses; the runtime has no GPU helper list).
  - `attach/nv_attach_impl/nv_attach_impl_frida_setup.cpp`: the launch geometry log (see `../e9-hooks/README.md`).
- Offline check of the regenerated trampoline (`trampoline-check/`: `.target sm_87`, `ptxas -c`, `cuobjdump
  -sass`): 520 is `VOTEU.ANY`; 521 reads `c[0x0][0x30..0x34]` (a launch constant); 522 reads `c[0x0][0x10c]`;
  **523 compiles to the immediate 48** (`HFMA2.MMA R4, -RZ, RZ, 0, 2.86e-06`, i.e. 0x0030): on sm_87 `%nwarpid`
  is a compile-time 48 (1,536 threads per SM / 32), the W of the design note.
- Configure: the dev build's cache options (gcc-13/g++-13, RelWithDebInfo, LLVM 15, CUDA 13.2, CUDA attach,
  LLVM and ubpf JIT, libbpf, kernel BPF, daemon, frida, probe read/write checks off) with
  `BPFTIME_ENABLE_UNIT_TESTING=OFF`; log `build-cuda/configure.log`. `core.cpp.o` and
  `nv_attach_impl_frida_setup.cpp.o` were compiled first.
- Build: `nohup timeout 14400 cmake --build build-cuda -j3 --target bpftime-agent bpftime-syscall-server
  ptxpass_kprobe_entry ptxpass_kretprobe ptxpass_kprobe_memcapture nv_attach_impl_ptx_compiler`, started
  04:01 JST (PID 1839502, in `build.pid`), done by 04:09 without errors (`build.log`). `bash preflight.sh`
  checks it.

# e9-hooks: do the launch hooks see every geometry before graph replays run it? (step 0, check 9)

Design note: `/home/jetson/src/results/gpu/dir1-design-space-20261115/README.md`, section 5, check 9:

> 9. **Launch-hook coverage** (B only; one logged run, then analysis).
>    - Log every geometry the hooks see in one pp512 + tg128 run with graphs on, and compare with the nsys rows per kernel and per graph.
>    - Decides whether B's counters grow before counted misses, or only after them.

Design B grows thread-indexed counters to 2H, where H is the largest geometry any hook has seen. Replays reach no
hook, so B's counters can grow before the first miss only if every geometry (at least each kernel's largest)
passes a hook before a replay runs it.

## Method

- **Geometry log** (instrumentation build, `../e7-llama/README.md` last section, commit `7e78a5d2`): with
  `BPFTIME_EXP_GEOMETRY_LOG=<file>`, each of the agent's nine launch-related hooks (`cudaLaunchKernel`,
  `cudaLaunchKernel_ptsz`, `cuLaunchKernel`, `cuGraphAddKernelNode` v1/v2, `cuGraphExecKernelNodeSetParams`
  v1/v2, `cuGraphKernelNodeSetParams` v1/v2) appends one tab-separated line as its first statement, before
  any early return (no impl, disabled, capturing stream):
  `ns tid depth hook name ptr gx gy gz bx by bz shmem capturing stream graph node`.
  - `ns`: CLOCK_MONOTONIC. `depth`: logged hooks already active on the thread (cudart's `cudaLaunchKernel`
    calls the hooked driver `cuLaunchKernel`, which logs at depth 1).
  - `name`: the mangled kernel name, cached per pointer. CUfunction: `cuFuncGetName`, else `cuKernelGetName`
    (a CUkernel may stand for a CUfunction since CUDA 12). Runtime pointers: `cudaGetFuncBySymbol`, and when that
    fails and the pointer is outside every loaded module, `cuKernelGetName`: nvcc 13's default launch passes a
    `cudaKernel_t` through `__cudaLaunchKernel`, which branches to `cudaLaunchKernel` (libcudart.so.13
    disassembly). The lookup is skipped (name `?`, not cached) while the application has a cudart error
    pending, and the error our lookup leaves is cleared, as `note_launch_if_probed` does.
  - `capturing`: `cuStreamIsCapturing` (1/0, `-` without a stream, `e<code>` on failure).
  - One `O_APPEND` fd opened at the first hook call, `write(2)` under a mutex. Without the variable nothing
    changes.
  - `naming-smoke/`: a CUDA 13 handle from `cudaGetKernel` is outside every module, `cudaGetFuncBySymbol`
    fails on it with 98 (and sets the last error), `cuKernelGetName` names it, and `cuFuncGetName` on it
    returns 400 without harm.
- **Runs** (`run_all.sh`, through `bpftime-gpu-work/run.sh`, `GPU_BUILD` = the instrumentation build, graphs
  on): llama-bench `-p 512 -n 0 -r 1 --no-warmup` and `-p 0 -n 128 -r 1 --no-warmup`, with the empty kretprobe
  on rms_norm (`example/gpu/threadhist-llama-empty`) so the agent's hooks are installed and the probe adds
  nothing. Two controls first: `example/gpu/cudagraph` (`vec_add_graph` and `vec_add_graph_legacy`, 3 replays,
  its own probe): one direct launch, then `cudaGraphAddKernelNode`, `cudaGraphKernelNodeSetParams`,
  `cudaGraphExecKernelNodeSetParams`.
- **Analysis** (`analyze_hooks.py --log L --sqlite S --out P --control CG1 --control CG2`): nsys rows of the
  census run with the same arguments (`bpftime-gpu-work/llama/census/{pp512,tg128}-graph.sqlite`,
  `CUPTI_ACTIVITY_KIND_KERNEL` joined with `StringIds` on `mangledName`, graph kernels traced per node) against
  the log, joined on the mangled name.
  - Hook lines are classed as direct (launch hooks, stream not capturing), capture (launch hooks, stream
    capturing), unknown (capture query failed), node_add, node_set, exec_set. Capture and node classes happen
    before any replay of that graph ("pre-replay", structurally). Lines at depth > 0 (cudart's
    `cudaLaunchKernel` calls the hooked `cuLaunchKernel`) count for geometry sets, not for line counts.
  - Each nsys kernel row is linked by `correlationId` to the runtime API call that launched it and classed
    as graph (`cudaGraphLaunch`), hooked (a launch API the agent hooks) or unhooked (any other, e.g.
    `cudaLaunchKernelExC`: a coverage gap by construction). Hooked launch calls with no kernel row are the
    launches recorded into a graph during capture.
  - Per kernel: launches by class, thread executions; distinct geometries in nsys and at the hooks; the largest
    geometry (threads per launch) overall and among graph launches, and the hook classes that saw each;
    launches and thread executions whose geometry never reached a hook; graph launches whose geometry was seen
    pre-replay, only directly, or nowhere. Per nsys graph: nodes, replays, nodes seen pre-replay. Globally: the
    largest threads per launch in nsys, at any hook, and at pre-replay hooks (one counter map serves every
    probed kernel).
  - `--control`: the cudagraph logs must each show a direct launch (depth 0, not capturing) and one line each
    of node_add, node_set and exec_set, all named `_Z9vectorAddPKfS0_Pf` with grid 4096x1x1 and block 256x1x1
    (`vec_add_graph.cu`: 1 << 20 elements, 256 threads per block). The llama runs use no graph-node API, so
    these controls are the only evidence that the node hooks log at all.
  - Outputs `<P>.summary.txt`, `<P>.summary.json`, `<P>.kernels.csv`.

## Decision rules (written 2026-11-15 before any real run; amended by the review the same day, before any main-session run)

Per test (pp512, tg128). **Validity**: any failure makes the verdict INCONCLUSIVE (the runs differ, the log
lost lines, or the instrument is wrong); `validity_detail` says which numbers disagree.
- V0: the log has lines, every name resolves after filling names by pointer, no capture-status query failed.
- V1: depth-0 runtime-launch lines = nsys hooked launch API calls (pp512: 444; tg128: 848).
- V2: depth-0 capturing lines = nsys hooked launch calls that launched no kernel row, i.e. were recorded into
  a graph (pp512: 0; tg128: 424 = 314 + 110 nodes of the two graphs).
- V3: depth-0 graph-node lines = nsys graph-node API calls, per class (llama: 0).
- V4: per kernel, depth-0 direct lines = nsys kernel rows launched directly by hooked APIs. (Capture plus node
  lines at least the kernel's graph nodes is reported, not required: nodes recorded through an unhooked API
  fall short there by design, and V2 checks the capture total.)
- V5: every (kernel, geometry) seen at a hook was also run in the nsys run (no kernel only at the hooks).
- V6: both cudagraph controls pass (missing controls fail it).

**Verdict** when valid:
- **YES** (B's counters can grow before counted misses on this workload): every kernel nsys ran had its
  largest geometry at some hook, and every kernel with graph launches had its largest *graph* geometry at a
  capture or graph-node hook.
- **NO** otherwise, listing the kernels: NO for a kernel whose largest geometry never reached a hook; NO-direct-only
  for a graph kernel whose largest graph geometry was seen only at direct launches (its growth would depend on
  that direct launch preceding the replays, which two separate runs cannot show). nsys kernel rows launched by
  unhooked APIs are listed with the verdict.
- Also reported, not part of the verdict: the strict form (every graph launch's own geometry seen pre-replay,
  every geometry seen), per-kernel capture-line counts against graph nodes, and per-graph coverage.

Amendments by the review (before any main-session run): V1-V6 were reported but not enforced in the writer's
version, so a hook run with graphs off returned NO-direct-only instead of INCONCLUSIVE, a failed capture
query counted as a direct launch, and the cudagraph control was printed but never judged. The pre-replay
requirement now uses the largest *graph* geometry (before, a kernel whose largest geometry ran only directly
would have been NO-direct-only).

Controls that show the test can fail: `smoke_analysis.sh` builds logs (and, where needed, a modified copy of
the census sqlite) from the census itself, 11 cases: complete (YES on both tests); graphs off at the hooks
(INCONCLUSIVE on tg128: V2, V4); the largest direct geometry launched through an unhooked API (NO on pp512);
the second capture recorded through an unhooked API (NO on tg128, 3 kernels NO-direct-only); names blanked
(INCONCLUSIVE); capture status unknown (INCONCLUSIVE); a control log without node lines and no control at all
(INCONCLUSIVE). On the board, the two cudagraph controls (V6).

## Commands (for the main session; geometry only, no timing)

| what | command | wall |
|---|---|---|
| analysis smoke, no GPU (done) | `bash smoke_analysis.sh` | 30 s |
| board smoke (done, below) | `timeout -k 10 600 bash smoke_run.sh` (`PARTS="cg cg-legacy pp32 tg128"`) | ~2 min |
| controls only (done: pass, `results/controls-only-1115-050123`) | `CONTROLS_ONLY=1 timeout -k 10 300 bash run_all.sh` | 40 s |
| **full run** | `timeout -k 10 1800 bash run_all.sh` | ~2-3 min |
| analysis again | `python3 -I analyze_hooks.py --log results/<stamp>/geometry-tg128.tsv --sqlite /home/jetson/src/bpftime-gpu-work/llama/census/tg128-graph.sqlite --out results/<stamp>/tg128 --control results/<stamp>/run-cg-default/geometry.tsv --control results/<stamp>/run-cg-legacy/geometry.tsv` | 5 s |

`run_all.sh` runs the same preflight as `../e7-llama` (copied here) and nothing if it fails; it checks the two
cudagraph controls right after they run (`analyze_hooks.py --controls-only`) and runs no llama-bench if
either fails (V6 would make every verdict INCONCLUSIVE). Run directories:
`bpftime-gpu-work/runs/step0-e9-<label>-<stamp>` (linked from `results/<stamp>/`); each has `geometry.tsv`.

## Smoke results (2026-11-15 04:14-04:53 JST, board shared; geometry does not depend on load)

- **tg128, graphs on** (`smoke/run-1115-041640`, a full-size decode run): **YES**. 848 launch-hook lines at
  depth 0 (424 direct, 424 capturing) plus 848 nested `cuLaunchKernel` lines (depth 1, the same split), no
  graph-node lines, 0 unresolved names. 13 kernels in both runs, per-kernel counts match nsys, every geometry
  seen. Both nsys graphs (314 and 110 nodes, 127 replays each) had all their node geometries logged during
  capture; the largest launch (4,096,000 threads, `mul_mat_vec_q` type 14, the output projection) was seen at
  capture.
- **pp32** (`smoke/run-1115-041459`): YES, 488 lines at depth 0 (all direct) and 488 nested, 17 kernels in
  both runs, every geometry seen.
- **cudagraph control, default launch**: 5 lines, all named `_Z9vectorAddPKfS0_Pf`, grid 4096x1x1, block
  256x1x1: `cudaLaunchKernel` (+ nested `cuLaunchKernel`), `cuGraphAddKernelNode_v2`,
  `cuGraphKernelNodeSetParams_v2`, `cuGraphExecKernelNodeSetParams_v2`. The runtime graph-node calls reach the
  hooked driver entry points.
- **cudagraph control, legacy launch** (`smoke/run-1115-042336`, host-stub pointers: the
  `cudaGetFuncBySymbol` path, whose lookup can load the module inside the log call): the same 5 lines and
  names; the agent still loaded the patched image, the victim printed 3 correct results (`client_rc=0`) and
  the probe counted 4 launches (1 direct, 3 replays).
- Analysis smoke, writer's version: its four fabricated cases gave the expected verdicts (replaced by the
  review's 11 cases in `smoke/fake/`).

- Review (04:53 JST): the cudagraph controls again (`smoke/run-1115-045310`, default and legacy launch): the
  same 5 lines each, both pass V6. `smoke/recheck-1115/`: the writer's tg128 and pp32 logs re-analyzed with
  V0-V6: all pass (tg128: 848 runtime lines = 848 hooked calls, 424 capturing = 424 recorded, no node lines,
  no unhooked API rows, strict form true), verdict YES on both. The pp512 hook run has not been executed.
- `smoke_analysis.sh` (review version): all 11 fabricated cases give the expected verdict.

## Known limits

- The hook run and the nsys run are separate processes; llama-bench's kernel sequence is deterministic for a
  given model and arguments (checked by kernel sets and per-kernel counts).
- "Before replay" is structural: capture and node-hook lines precede any replay of their graph. It says a hook
  could learn the geometry in time; growing storage there (writing a control block the device reads) is not
  tested.
- One workload, one model, batch 1. In this run each graph was captured once and replayed without
  `cudaGraphExecUpdate`. A recapture would pass through the launch hooks again before its replays, but
  `cudaGraphExecUpdate` itself is not hooked.
- Not hooked, not used in these runs (nsys runtime API): `cudaLaunchKernelExC` / `cuLaunchKernelEx` (llama.cpp's
  PDL path, off on sm_87), `cudaLaunchCooperativeKernel` (softmax's column split).
- The log costs a name lookup per new pointer, a `cuStreamIsCapturing` and a `write(2)` per hook entry; the
  runs measure geometry, not time.
- Capture lines carry no graph handle, so a graph node counts as seen before replay when its (kernel, geometry)
  appears at any capture or node hook; with each graph captured once (V2 holds) this is the node's own capture.
- The cudagraph controls exercise the direct and graph-node hooks, not stream capture; the capture path is
  checked by the llama run itself (V2: capturing lines = launches nsys recorded into graphs).
- The NO paths (a geometry launched or captured through an unhooked API) are tested on fabricated data only;
  no board run launches through an unhooked API.
- `run.sh` names each run's segment `bpftime_gpu_<NAME>` (not `step0_`; `run.sh` is shared and was not changed)
  and removes it; the scripts diff `/dev/shm`.

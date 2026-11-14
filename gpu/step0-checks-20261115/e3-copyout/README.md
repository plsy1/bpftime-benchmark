# Step 0, check 3: device memory to the reader

Design note: `/home/jetson/src/results/gpu/dir1-design-space-20261115/README.md`, section 5, check 3:

> 3. **Device memory to the reader** (A, C, D storage; B's record pool).
>    - An agent-side cuMemcpyDtoHAsync on its own stream into the segment while llama.cpp runs with graphs (ordering, cost).
>    - A POSIX-FD VMM export mapped by a second process.
>    - Decides whether storage the CPU reads can live in device memory on the Orin at all.

Everything here is standalone (no bpftime). The programs were built and smoke-run only (short runs on a shared GPU).
No result in this folder is a measurement. The main session runs the real batch with `run_all.sh` on a quiet board.

Terms used below:
- **segment-like host memory**: POSIX shm + `mmap(MAP_SHARED)` + read prefault + `cuMemHostRegister(DEVICEMAP | PORTABLE)`. This is how bpftime registers its segment (dev tree `runtime/src/bpftime_shm_internal.cpp:1414-1469`). The shm name (`/step0_e3_*`) is unlinked right after `mmap`, so nothing stays in `/dev/shm`.
- **chunk**: one record unit in device memory. It holds a payload of 32 lanes x `wpl` 8-byte words (2 KiB at `wpl`=8, 16 KiB at `wpl`=64) plus one flag word.
- **flag**: the sequence number of the last complete payload of a chunk.
- **sequence**: the round number of a writer warp. Payload word `i` of chunk `c` holds `(seq << 32) | (c x words_per_chunk + i)`.
- **violation**: in one copied snapshot, a chunk whose flag says `s` but one of whose payload words is older than `s`, or sits at the wrong position. Newer words are legal.
- **mixed chunk**: a copied chunk whose payload words carry different sequences. The copy then read the chunk while the writer was writing it, which is the evidence of overlap.
- **copier**: the thread in the LD_PRELOAD shim (part B) that copies device memory to host memory at a set rate.
- Copier configurations in part B:

  | name | bytes per copy | period | asks for |
  |---|---|---|---|
  | `noshim` | no shim loaded | - | - |
  | `idle` | 0 (thread, context, buffers; no copies) | - | - |
  | `c1m1ms` | 1 MiB | 1 ms | 1.05 GB/s |
  | `c16m1ms` | 16 MiB | 1 ms | 16.8 GB/s; expected to saturate |
  | `cont` | 16 MiB | back to back | as much as the copy engine gives |

## Files

| path | what |
|---|---|
| `build.sh` | builds `build/{copyorder,captest,libstep0_copier.so,vmmexport}`, keeps PTX (`build/keep*/`) and SASS, runs `check_sass.sh` |
| `check_sass.sh` | checks that the stores, fences, loads and interposed symbols exist in PTX, SASS and dynamic symbol tables, and the writer's store/fence order (31 checks) |
| `preflight.sh OUT` | read-only: clocks (cpu5 1728000 kHz, GPU 1020000000 Hz), possible GPU users, `step0_` shm. Fails unless clean (`ALLOW_DIRTY=1` overrides) |
| `common/regmem.h` | segment-like host memory |
| `partA/copyorder.cu`, `partA/run.sh` | part A: ordering of a concurrent copy, and DtoH latency and throughput |
| `partB/step0_copier.c` | part B: the shim (copier thread + graph-API interposers) |
| `partB/captest.cu`, `partB/run_b0.sh` | part B0: the shim next to a capture-and-replay loop (capture modes, copier operations) |
| `partB/run_llama.sh` | part B: llama-bench pp512/tg128, five configurations, interleaved rounds, graphs-off control |
| `partB/run_nsys.sh`, `partB/nsys_parse.py` | part B: nsys cross-check of graph counts and copy/kernel overlap |
| `partC/vmmexport.cu`, `partC/run.sh` | part C: POSIX-FD VMM export to a second process |
| `summarize.py OUT` | applies the decision rules below; writes `OUT/summary.md` and `OUT/decisions.json` |
| `run_all.sh` | preflight, `check_sass`, A, B0, B, nsys, C, postflight, summary |
| `smoke/` | smoke outputs (short, on a shared GPU; not results); `smoke/review-20261115/` holds the review's re-smoke |

## How to run (main session, quiet board)

```
cd /home/jetson/src/bpftime-gpu-work/step0/e3-copyout
./build.sh                                  # about 1 min, no GPU work
timeout -k 10 1800 ./run_all.sh             # about 9 min; output in out/<date-time>/
```

To run the parts one by one, use one OUT directory:

```
O=out/manual-1; mkdir -p $O
bash preflight.sh $O
timeout -k 10 600  bash partA/run.sh $O      # about 2 min (48 variants x SECS=2)
timeout -k 10 900  bash partB/run_b0.sh $O   # about 1 min (14 cases x SECS=3)
timeout -k 10 1200 bash partB/run_llama.sh $O  # about 4 min (ROUNDS=3, REPS=3)
timeout -k 10 900  bash partB/run_nsys.sh $O   # about 1 min (2 nsys runs + export)
timeout -k 10 600  bash partC/run.sh $O      # about 30 s (4 variants x SECS=3)
python3 -I summarize.py $O
```

Every GPU command inside the scripts has its own `timeout -k 5`, outside any `env LD_PRELOAD=...`. Every device loop is capped:
- part A's writer has a per-launch round cap and a `%globaltimer` deadline (10 s), and counts its exit reasons;
- the shim's event poll is capped at 500,000 polls;
- part C's processes use `alarm()` and bounded socket waits.

Knobs:
- `SECS` (A, B0, C);
- `ROUNDS`, `REPS` (B);
- `STEP0_WAIT=event` exported before `run_llama.sh` makes the copier poll an event instead of blocking in `cuStreamSynchronize`;
- `SKIP_A`, `SKIP_B0`, `SKIP_LLAMA`, `SKIP_NSYS`, `SKIP_C` for `run_all.sh`;
- `LLAMA_BENCH=<path>` replaces the llama-bench binary in `run_llama.sh` and `run_nsys.sh`, for a dry run with a stub.

## Part A: ordering of a copy that runs concurrently with the writer

**Question.** A writer kernel on stream S1 keeps filling chunks in device memory. A copier on a separate non-blocking stream S2 copies
the chunk area with `cuMemcpyDtoHAsync` into segment-like host memory. The CPU checks every copied chunk: when the flag says
sequence `s` is complete, is every payload word of `s` in the same copy?

**Method** (`copyorder order`):
- **Writer.** 64 blocks x 128 threads (256 warps). Warp `w` owns chunks `w, w+256, ...` and loops over sequences. Per chunk:
  1. every lane stores its `wpl` payload words;
  2. every lane runs `__threadfence()`;
  3. `__syncwarp()`;
  4. lane 0 runs `__threadfence()` and stores the flag (volatile).

  Each lane fences its own stores, so this does not test check 4's leader-fence question.
- **Layouts.** They are named for where the flag sits relative to its payload:
  - `after`: `[payload][flag line]`, one copy;
  - `before`: `[flag line][payload]`, one copy;
  - `sepff`: flags in a separate array; the two-step protocol copies the flags, then the payload, on S2 (stream order);
  - `seppf`: the same arrays, copied payload first. This order must race; it is a control.
  - `farafter`: one allocation `[all payload][all flags]`, one copy. Every flag lies far above its payload, so the copy reads it much later than its payload.
  - `farbefore`: one allocation `[all flags][all payload]`, one copy.

  `after` and `before` put the flag only 2 or 16 KiB from its payload, which the copy may cross faster than the writer's fence-and-flag sequence. The far layouts show whether one copy is safe whatever the distance.
- **Writers.**
  - `normal`: as above.
  - `reversed`: lane 0 stores the flag before the payload. This is the can-fail control: a reader can see the flag without its data.
- **Matrix.** 2 areas (1 MiB and 16 MiB of payload) x 2 chunk sizes (`wpl` 8 and 64) x 6 layouts x 2 writers = 48 variants, 2 s each.
  After the host stops the writer, one more copy is the quiescent control: every flag must equal its warp's last sequence and every chunk must be exact.
- **Writer cap path.** `order-seqcap.jsonl` runs with a 64-round cap per launch, so the writer must end by its cap, be relaunched and keep its sequence.
- **Latency and throughput** (`copyorder bw`): 64 KiB, 1 MiB and 16 MiB, each with the writer idle and with it running.
  - Latency: 200 single copies; host wall time from issue to the return of `cuStreamSynchronize`, plus event time.
  - Throughput: back-to-back copies (1000, 200 and 30).

  All buffers are allocated before the writer starts. A smoke showed that `cuMemFree` and `cuMemHostUnregister` wait for a running kernel.

**Decision rules** (per run; `summarize.py` implements them):
- A run is **valid** if all of these hold:
  - the quiescent snapshot has 0 violations, 0 mixed chunks, 0 flag mismatches and 0 unflagged chunks;
  - the writer was overlapped: mixed chunks > 0, and some flag changed in at least 90% of the copies;
  - the writer ended only by the host's stop word (0 cap exits, 0 deadline exits);
  - a can-fail control with the same area and chunk size has at least 1 violation: the `reversed` run of the same layout, or the normal-writer run of its mirror (the same data read in the opposite order: `after`/`before`, `farafter`/`farbefore`, `seppf`/`sepff`).
- For each layout, with the normal writer:
  - **violates**: any run whose quiescent check passed has a violation;
  - **consistent**: every run is valid, has 0 violations, and together they have at least 1,000,000 flagged chunk observations and 10,000 mixed chunks;
  - **inconclusive**: otherwise.
- If `seppf` with the normal writer shows no violation, the copies did not overlap the writer enough to catch a real race. Every "consistent" is then downgraded to inconclusive.
- **Answer** to "does a copy that sees a complete flag also see the payload?":
  - **yes**: all four single-copy layouts (`after`, `before`, `farafter`, `farbefore`) are consistent. One copy is enough whatever the flag's side and distance.
  - **fixable**: otherwise, `sepff` is consistent. Copy-out needs the two-step copy, flags first, in stream order (a defined protocol).
    - Single-copy layouts that passed are listed but not relied on. The copy engine's read order is not documented, and another placement of the same data failed.
    - For example, a clean `after` with a violating `farafter` means "small chunks only".
  - **no**: `sepff` violates. Copy-out would need events or kernel boundaries.
  - **inconclusive**: anything else.
- Note (review, 2026-11-15): `after` and `farafter` read each flag after its payload. A writer that completes a later round
  between those two reads makes the payload older than the flag, whatever its fences, so their violations are a reader-side race
  (the same race `seppf` shows on purpose). "yes" is reachable only if the copy behaves as a snapshot; the expected best answer is
  "fixable". In `after` the gap is only 2 or 16 KiB, so a clean `after` can be timing luck.
- **Sub-verdict, flag-first single copy** (`flag_first_single_copy` in `decisions.json`): `consistent` if `before` and `farbefore`
  are both consistent, `violates` if either violates, else `inconclusive`. It says whether one copy works when every flag sits at a lower
  address than its payload. It rests on the copy engine reading in ascending address order, which is observed here, not documented.
  It does not change the answer above.
- Throughput is reported, not judged. It is compared with the 0.9-6.2 GB/s that one record per warp would offer (design note section 6).

## Part B0: the copier next to a capture loop (standalone)

**Why.** llama.cpp captures only a few graphs per run (the census tg128 run made 2 captures and 254 `cudaGraphLaunch` calls), so a
copier in llama-bench rarely overlaps an open capture. `captest` keeps a capture open 30-100% of the time:
- `cudaStreamBeginCapture`, then 200 kernel launches, then `cudaStreamEndCapture`;
- `cudaGraphInstantiate` the first time and `cudaGraphExecUpdate` after it;
- `cudaGraphLaunch` and a synchronize.

It counts every call's result instead of aborting. It is linked with the shared cudart, as `libggml-cuda.so` is, so the shim's
interposers see the same calls.

**Cases** (`run_b0.sh`, 3 s each; the copier copies 1 MiB back to back unless noted):
- no shim;
- shim idle;
- async copy (`cuMemcpyDtoHAsync` + `cuStreamSynchronize` on the shim's non-blocking stream) in relaxed, global and thread-local capture modes;
- async copy with event polling, relaxed and global;
- `cuMemcpyDtoH` on the legacy stream, as 1502's `elem_lookup` does today: relaxed, relaxed with a blocking capture stream, global;
- `cuMemAlloc`+`cuMemFree` before every copy, relaxed and global;
- `cuCtxSynchronize` before every copy, relaxed and global.

**Decision rules** (revised 2026-11-15, before any real run; see "Revisions"):
- **Interposition works**: in every case with the shim, its counts of `cudaStreamBeginCapture`, `cudaStreamEndCapture`, `cudaGraphInstantiate`, `cudaGraphExecUpdate` and `cudaGraphLaunch` equal captest's own counts.
- **The detector can fail in relaxed mode** (positive control): `legacysync-relaxed-blk` shows end-capture errors. There, a
  legacy-stream copy from another thread meets a blocking capture stream, which invalidates a relaxed-mode capture. The global-mode
  controls (`memalloc-global`, `ctxsync-global`) are reported, not required.
- **Collisions happened**: the shim counts `copies.during_capture`, copies during which a capture begun through the interposer had not
  ended. `async-relaxed` and `asyncevent-relaxed` each need at least 100 capture iterations and at least 1,000 such copies.
- **Verdict** for "the async copier is safe next to relaxed-mode captures":
  - **fail**: `async-relaxed` or `asyncevent-relaxed` crashed (rc other than a timeout), or showed captest errors or copy/wait errors
    in the shim, while the baseline cases `noshim` and `idle-relaxed` ran with 0 errors. Without a clean baseline it is inconclusive.
  - **pass**: neither failed, both ran with copies > 0, met the collision floor, interposition works, and the relaxed control fired.
  - **inconclusive**: otherwise (a control did not fire, too few collisions, a missing log, a timeout).
- Global-mode safety is reported separately (`global_safe`). It decides whether the copier is safe for applications that capture in
  global mode. llama.cpp is not one of them (below).

## Part B: copier shim inside llama-bench, graphs on

**Shim** (`partB/step0_copier.c`, C, no bpftime). It never writes to the application's stdout or stderr; it writes one JSON file at exit.
1. **Start.** A constructor starts the copier thread. The thread polls `cuDeviceGet` and `cuDevicePrimaryCtxGetState` every 2 ms
   (`CUDA_ERROR_NOT_INITIALIZED` means not yet). It never calls `cuInit`.
2. **Set-up**, once the application's primary context is active:
   - retain the context;
   - register `atexit` (so it runs before cudart's teardown);
   - create a non-blocking stream;
   - `cuMemAlloc` 16 MiB, and 16 MiB of segment-like host memory.
3. **Copy loop.** Copies start every `STEP0_COPY_PERIOD_US` on an absolute schedule; 0 means back to back.
   - A late copy starts at once, and the skipped periods are counted; there is no catch-up burst.
   - Each copy is `cuMemcpyDtoHAsync` followed by `cuStreamSynchronize`, or event polling with `STEP0_WAIT=event`.
4. **Exit.** The `atexit` handler stops the thread and joins it (5 s bound), then writes the JSON:
   - copies, bytes, achieved GB/s, missed periods, and `during_capture` (copies that overlapped an open capture);
   - per-copy latency (1 us histogram: p50, p90, p99, p99.9, max);
   - a 100 ms timeline of copies and latencies, so the model-load phase can be separated;
   - the set-up results;
   - the graph-API counters.

**Interposers.** `cudaStreamBeginCapture`, `cudaStreamEndCapture`, `cudaGraphInstantiate`, `cudaGraphExecUpdate` and `cudaGraphLaunch` are
forwarded with `dlsym(RTLD_NEXT)`, and their calls, errors and capture modes are counted. `libggml-cuda.so` imports all five from
`libcudart.so.13` dynamically (`check_sass.sh`). The smoke run showed that the shim's unversioned definitions bind to these versioned references.

**What llama.cpp does with graphs** (`L/ggml-cuda.cu:5284, 5475`; `graph.cuh:66`; `L` as in the design note):
- Both capture sites use `cudaStreamCaptureModeRelaxed`.
- Every graph call is wrapped in `CUDA_CHECK`, so an invalidated capture aborts the process; there is no quiet fallback.
- Graphs are on when `GGML_CUDA_DISABLE_GRAPHS` is unset.

**Detection method for "graphs still work"** (every run, plus one control and one cross-check):
1. **Per-run counters.** The shim's counters are in every run with the shim.
2. **Abort check.** `rc` and "CUDA error" in stderr catch an abort.
3. **Graphs-off control.** One run with `GGML_CUDA_DISABLE_GRAPHS=1` and the shim idle must count 0 captures and 0 launches. This shows the detector can say no.
4. **nsys cross-check** (`run_nsys.sh`, tg128 -r 1, shim idle and `cont`; `--cuda-graph-trace=node`, as the census used). It counts:
   - kernels run inside graphs (`graphNodeId` set);
   - the runtime API's `cudaGraphLaunch` rows, which must equal the shim's count (and be > 0); a mismatch makes `graphs` inconclusive;
   - the shim's 16 MiB DtoH copies, and the share of their time that overlaps kernel execution.

**Runs** (`run_llama.sh`):
- **Matrix.** 3 rounds; in each, the 5 configurations in an order rotated by one per round.
- **Each configuration, per round:** `llama-bench -m tinyllama Q4_K_M -r 3 -o json`, with the default warm-up, run once with `-p 512 -n 0` and once with `-p 0 -n 128`.
- **Recorded per run:** the clocks before and after.

**Decision rules:**
- **Graph gate**, for each graphs-on run with the shim:
  - rc 0 and no "CUDA error";
  - a shim log, with 0 errors on all five graph calls;
  - `cudaGraphLaunch` count equal to the `idle` run of the same round and test;
  - for copying configurations, copies > 0 with 0 copy or wait errors.
- **Graphs keep working** (`graphs`: yes / no / inconclusive; revised 2026-11-15):
  - **no**: a run broke the gate by a failure: rc other than a timeout, "CUDA error", graph-call errors, a launch count that differs
    from the idle run's, or copy/wait errors, while every no-shim run ended with rc 0 and no "CUDA error" (else inconclusive).
  - **inconclusive**: no failure, but a gap: a missing shim log, a timeout (rc 124 or 137), a copier that made no copies, no idle run
    to compare with, a graphs-off control that did not show 0 captures and 0 launches, round-1 tg128 with 0 launches, or an nsys
    cross-check whose `cudaGraphLaunch` rows differ from the shim's count or whose parse output is missing for a tag (nsys not run at
    all leaves the verdict to the other checks).
  - **yes**: otherwise.
- **Noise**: per test, (max - min) / mean of the three `noshim` round means.
  - **Loss**: (noshim mean - config mean) / noshim mean.
  - **Idle shim OK**: loss is at most max(1%, noise) for both tests.
- **Bounded throughput loss**: `c1m1ms` loses at most 5% in both pp512 and tg128.
  - 1 MiB every 1 ms is about 1.05 GB/s, near the low end of the 0.9-6.2 GB/s offered by one record per warp.
  - The verdict is inconclusive if the noise exceeds 3%, or if `c1m1ms` achieved fewer than 500 copies/s (it then did not ask for its rate).
- `c16m1ms` and `cont` are stress points: they are reported, not judged.
- **Part B passes** if the graphs keep working (yes), the loss is bounded (yes), and B0 passes. It **fails** if graphs is no, bounded
  is no, or B0 fails; otherwise it is inconclusive.

## Part C: POSIX-FD VMM export to a second process

**Method** (`vmmexport`, one `fork()` before any CUDA call; parent = process 1, child = process 2; fd over a socketpair with `SCM_RIGHTS`):
- **Process 1 (exporter):**
  1. `cuMemCreate` 16 MiB, device location, `requestedHandleTypes = CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR`;
  2. reserve, map, `cuMemSetAccess` (device);
  3. fill with a kernel, generation 1;
  4. `cuMemExportToShareableHandle`, then send the fd;
  5. after process 2 is ready, rewrite the whole buffer with generation + 1 about every 2 ms until process 2 is done;
  6. synchronize and say "stopped", then wait until process 2 has scanned before releasing.
- **Process 2 (importer)** records the fd's `/proc/self/fd` link and `fstat`, then:
  1. `mmap`s the fd before any CUDA call;
  2. `cuMemImportFromShareableHandle`, `cuMemGetAllocationPropertiesFromHandle`, reserve, map, `cuMemSetAccess` (device);
  3. (i) reads it with a kernel: words not equal to generation 1's pattern;
  4. (ii) `cuMemcpyDtoH` into pageable and into segment-like memory: wrong words, plus 20 x 16 MiB timed copies from the imported memory and from a local `cuMemAlloc` buffer (GB/s, latency);
  5. (iii) tries CPU access: `cuMemSetAccess` with `CU_MEM_LOCATION_TYPE_HOST` and `HOST_NUMA`, `cuPointerGetAttribute(HOST_POINTER)`, `mmap` of the fd again after import;
  6. **live phase:** every ~1 ms it samples the first and last word by DtoH, by a kernel and through the CPU mapping, counting distinct generations, backward steps and mixed samples;
  7. **quiescent phase**, after process 1 stopped (its context still alive): a full DtoH check, and full CPU-mapping scans at 0, 10, 100 and 1000 ms. Each scan is plain, and again with `dc civac` per line, which rules out stale CPU cache lines. One more scan follows after process 1 exited.
- **Variants:**
  - fill with plain stores;
  - `st.global.wt`, which lowers to `STG.E.64.STRONG.SYS`;
  - plain stores + `__threadfence_system()` (`MEMBAR.SC.SYS`);
  - `allocFlags.gpuDirectRDMACapable = 1`.
- Every driver call's `CUresult` is in the JSON.

**Decision rules:**
- **Tier 1, direct CPU mapping usable.** For at least one variant:
  - the fd maps and reads without a fault;
  - 0 stale words in the generation-1 scan and in the quiescent scans at 0 and 10 ms;
  - the CPU mapping saw at least 50% of process 1's generations, with no backward step.
- **Tier 2, import + reads in the reader's own context.**
  - import, map and set-access succeed;
  - the kernel and DtoH checks of generation 1 find 0 wrong words;
  - DtoH saw at least 50% of the generations, with no backward step (DtoH and kernel);
  - the final DtoH check finds 0 stale words.

  The reader process then needs its own CUDA context and a copy per read, as 1502 does on a discrete GPU.
  Tier 2 is still a DtoH copy that runs concurrently with the writer, so its ordering answer is part A's. Its cost on a real
  application is not measured here, because process 1 is a toy writer. The summary prints both qualifiers next to this path.
- **Part C works** if tier 1 or tier 2 holds in some variant. The summary names the tier.
- **Part C is no** only if every variant has both processes' records (exporter, importer, parent) and none holds a tier; a variant
  whose process crashed or hit its alarm decides nothing, so C is then inconclusive (revised 2026-11-15).

## Overall decision

"Can storage the CPU reads live in device memory on the Orin at all?"
- **Yes** if (part A answers yes or fixable AND part B passes, B0 included) OR part C works (tier 1 or 2).
  This follows the task's rule. Tier 2 alone makes the answer yes, but it does not settle ordering (part A) or cost on an application (part B).
- The summary lists which of three paths passed:
  1. agent copy-out into the segment;
  2. direct CPU mapping of the exported fd;
  3. reader-side import and copy.
- **No** if path 1 (agent copy-out) failed on a verdict (A is "no", or B or B0 failed) and part C is "no".
  A failed control or a missing part never gives "no"; it gives inconclusive (revised 2026-11-15).
- **Inconclusive** otherwise.
- `decisions.json` holds `path1_copy_out` (pass / fail / inconclusive) next to the overall answer.

## Revisions before the first real run

2026-11-15, adversarial review (no real run had been made; smoke runs only). Each change has a structural reason:
1. B0's required positive control moved from global mode to relaxed mode (`legacysync-relaxed-blk`), because the claim under test is
   relaxed-mode safety. The global controls stay in the table.
2. B0 needs evidence of collisions: the shim now counts `copies.during_capture`, with floors of 100 capture iterations and 1,000
   colliding copies per relaxed safety case. Before, `copies > 0` was enough, which a copier that never met an open capture would pass.
3. Failed controls, missing logs, timeouts and incomplete part C variants now give "inconclusive", never "no" (B0, B, C, overall).
   Before, for example, a graphs-off control that failed made B "not ok", which could turn the overall answer into "no".
4. The nsys cross-check is enforced: a mismatch makes the graph verdict inconclusive (the README already said "must equal").
5. Part A keeps its answer rule; the note on `after`/`farafter` and the flag-first sub-verdict were added.
6. A "fail" needs a clean baseline: B0 fails only if `noshim` and `idle-relaxed` ran with 0 errors, and part B's graph verdict is
   "no" only if every no-shim run ended with rc 0 and no "CUDA error"; otherwise both are inconclusive. The nsys check has three
   states: not run (no `partB-nsys/`), match (both tags parsed and equal), mismatch or a tag missing/unparsable (inconclusive).
7. `run_b0.sh` quotes `shim_env` in `runs.csv` (it holds commas, which shifted the `rc` column); `summarize.py` reads rc as the last column.
8. `copyorder` no longer adds a launch's exit counters twice when the writer ended by its cap at the moment the run timed out
   (affected only the seq-cap check). `check_sass.sh` also checks the order payload, fence, flag (normal) and flag, fence, payload
   (reversed) in the writer's SASS.

## Known limits

- **Part A**
  - The writer is a synthetic kernel that writes device memory continuously. It is not probe code inside llama.cpp kernels, and with the writer running it is a worst case for DRAM traffic.
  - Only 2 KiB and 16 KiB chunks are tested, each written by one warp.
  - The copy engine is not a PTX thread, so no documented model says it observes the writer's fences in order.
    - "consistent" is evidence from this board, driver and toolkit (CUDA 13.2, driver 595.78), not a guarantee.
    - Only `sepff` rests on a documented rule (stream order between two copies).
  - The writer fences with `__threadfence()` (`membar.gl`, `MEMBAR.SC.GPU`), not the `membar.sys` that bpftime's helpers issue. A "no"
    (`sepff` violates) is established only for gpu-scope fences; rerun `sepff` with a system-scope fence before acting on it.
  - Newer payload words count as legal because this writer overwrites chunks at once. A record design that reuses a slot only after
    the reader released it (C's chunk states) never shows them; a design that overwrites in place needs a seqlock re-check (flags,
    payload, flags again), which is not tested here.
- **Part B**
  - The copier copies a buffer nobody writes. It measures interference (copy engine, DRAM, driver locks) and graph safety; part A measures ordering.
  - llama.cpp captures in relaxed mode. Applications that capture in global mode are covered only by B0.
  - With few captures per llama run, an unsafe copier could pass part B by chance. B0 is the test with many capture collisions.
  - `cuStreamSynchronize` in the copier may keep a CPU core busy, so tg128 may see CPU contention. Rerun with `STEP0_WAIT=event` to separate the effects; a B "no" (bounded loss) with the sync wait should be confirmed that way before it counts.
  - Latency statistics include the model-load phase. `timeline_100ms` separates it.
- **B0**
  - `cuCtxSynchronize` in the copier while captest captured in relaxed mode crashed in libcuda in a smoke (SIGSEGV in `cuCtxSynchronize`, backtrace in `smoke/gdb/`). `run_b0.sh` records that case as "crashed" and disables core files. In the review's re-smoke (SECS=0.5, `smoke/review-20261115/partB0/`) the same case did not crash, but every capture ended invalidated (901). In both smokes (short, shared GPU; not results) an agent-side `cuCtxSynchronize` during a relaxed capture would have broken llama.cpp's graphs (its `CUDA_CHECK` aborts).
  - In llama-bench itself almost no copy meets an open capture (3 of 1,079 copies in the review's `-p 32 -n 0 -r 1` smoke, `smoke/review-20261115/llama/`), which is why B0's collision floor carries the safety claim.
- **Part C**
  - Both processes come from one `fork()` without `exec`, and their contexts time-slice one GPU (no MPS).
  - The live sampling reads two words per sample, not whole snapshots.
  - Tier 2 needs CUDA in the reading process. Whether bpftime's loader or syscall server can host that context is not tested here.
  - The `gpuDirectRDMACapable` variant returned `CUDA_ERROR_INVALID_DEVICE` from `cuMemCreate` in the smoke run. It stays in the matrix as a recorded result.
- **Validation of the llama scripts.** `run_llama.sh` was validated only by a dry run with a stub llama-bench (`LLAMA_BENCH=<stub>`: 31 runs, rotated order, the graphs-off variable only on the control) and by one hand-typed smoke (`-p 32 -n 0 -r 1`) with the shim at 1 MiB per 1 ms. `run_nsys.sh` was smoke-run only with `captest`, not with llama-bench. After the review changed the shim (collision counter), one more `-p 32 -n 0 -r 1` smoke ran with rc 0, and `run_nsys.sh` was re-smoked on `captest` (nsys and shim `cudaGraphLaunch` counts equal; `smoke/review-20261115/nsys-captest/`). `summarize.py`'s verdict paths were exercised on synthetic trees built from the real llama-bench JSON and shim log (14 scenarios: pass, each failure, each gap).
- **Smoke runs.** All smoke runs were short, on a GPU that other agents shared. Smoke outputs in `smoke/` show only that the programs run and that the counters are sane; their numbers are not results.

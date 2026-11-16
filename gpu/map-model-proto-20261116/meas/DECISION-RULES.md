# Prototype measurement on llama.cpp (proto/map-model, 2026-11-16)

Written before the real runs; the decision rules below are fixed.

## Question

What does the per-thread map model's prototype cost on a real application,
against today's maps doing the same work, and does it count every thread?

## Builds and tools

- bpftime: branch `proto/map-model` (c2826dbc and later), worktree
  `/home/jetson/src/bpftime-proto`, build `build-cuda` (same options as dev's).
  Every variant, the old ones included, runs on this build (the prototype
  only adds map types; the old maps' code paths are dev 52126f33's).
- Tools (`proto/tools`), probes on four kernels at once (`*_llama4`):
  rms_norm, the largest mul_mat_q, the largest mul_mat_vec_q, rope (RoPE K),
  the kernels of step0/e7-llama.
  - `lat` (the model): the kernel entry stores %globaltimer in a thread-local
    map (1530); the thread's exit adds 1 to bucket log2(now - start) of a sum
    map (1531), per kernel. Exits without a start count in a 65th bucket.
  - `latmix`: the start in today's per-thread array (1502, created as 1512 on
    the Orin: registered host memory, one slot per thread index, cleared at
    exit), the histogram in the model's sum map.
  - `latold` (today): the start as latmix, the histogram in today's GPU array
    (1503, created as 1513: registered host memory), atomic adds.
  - `hist` (the model, threadhist): one u64 per kernel per thread index in
    device memory (1532), an atomic add per exit, delivered to the CPU every
    1 s while it changes.
  - `histold` (today's threadhist): 1502 (1512), `*cnt += 1`.
  - The same eBPF source for each pair (`pt_lat.bpf.c`, `pt_hist.bpf.c`);
    only the map types differ.
- Per-thread maps (1512, 1532) sized for 1,048,576 thread indices
  (`BPFTIME_MAP_GPU_THREAD_COUNT`): the largest launch has 445,644 threads
  (mul_mat_vec_q, tg128).

## Items

llama-bench (`build-ptx87`, TinyLlama 1.1B Q4_K_M), `-r 5 -o json`, with its
warmup (the agent loads and patches the modules during the warmup; the five
timed runs are steady state):

- pp512 (`-p 512 -n 0`): base, agent, lat, latmix, latold, hist, histold
- tg128 (`-p 0 -n 128`, CUDA graphs on): base, agent, lat, latmix, hist,
  histold. `latold` is not run on tg128: at its pp512 rate (about 0.6 us per
  thread exit, functional run) tg128's 3.14 x 10^9 exits per run would take
  over 30 minutes per run.

`base`: no agent (the loader runs). `agent`: the agent with a tool whose
probes are on kernels llama.cpp does not have (it patches nothing).

Two passes, serial, items shuffled per pass (seed = pass label), clocks fixed
(`jetson_clocks`: cpu5 1,728,000 kHz, GPU 1,020 MHz, checked before every
item), apt idle, nothing else on the board.

## Decision rules

1. **Correctness, per item** (`analyze.py`): every probed kernel's exits
   counted, against step0/e7-llama's per-run counts c: pp512 exactly 6c (the
   warmup is a full prompt); tg128 between 5c and 5c + c/64 (the warmup
   generates one token). Lat variants: no exit without a start. New maps: the
   final delivery done (status word 1). An item failing any check is
   INVALID and not used.
2. **Overhead** = 1 - (median of the five samples_ts) / (base's, same pass).
   Passes agree if a variant's overhead differs by at most 2 points between
   them; otherwise it is reported as unstable, both values given.
3. **Within the target**: `lat`'s overhead at most 5% on pp512 and on tg128,
   in both passes.
4. **Cheaper than today**: `lat` below `latold` (pp512) and not above
   `latmix` (both workloads), `hist` below `histold`, in both passes.
5. **Attribution** (reported, not judged): latmix - lat is the carry's
   share (thread-local start against 1512), latold - latmix the merge's
   (sum map against 1513).
6. **Memory** (reported): device bytes and pinned staging of the new maps
   (agent log), shared bytes of the old per-thread maps, local memory per
   thread (ptxas, `proto/ptx/ptxas_check.txt`), delivery count and time.

## Limits known in advance

- One application, four kernels; probes on all four at once.
- The agent's start-up (patching and compiling the modules) is in the
  client time, not in the samples; reported, not judged.
- `agent` patches nothing, so it bounds the agent's own effect only.

## Addendum (2026-11-17 00:0x, after passes p1 and p2, before p3 and p4)

p1 and p2 met rule 4 everywhere and rule 3 on pp512 only (`lat` on tg128:
31.9%). To tell the floor of probing every thread from the model's own cost,
two more passes (p3, p4) run these items, with `base` in each pass for the
reference; the rules for them, fixed now:

- `empty2`: an entry and an exit probe that do nothing, no map (lat's shape);
  `empty1`: an exit probe that does nothing (hist's shape);
  `carryonly`: `lat` without the histogram add (the start stored and read
  back, the bucket computed; a branch that never adds consumes it).
- `lat` and `hist` run again in p3 and p4, so every difference below is
  within one pass.
- Attribution, reported, not judged: the floor = empty2 (lat) or empty1
  (hist); the carry = carryonly - empty2; the merge = lat - carryonly;
  hist's add = hist - empty1. Each with both passes' values; a difference
  between p3 and p4 above 2 points is reported as unstable. lat and hist in
  p3/p4 must agree with p1/p2 within 2 points, or that is reported.

## Addendum 2 (2026-11-17, after the review fixes 606e74a5, before p5 and p6)

The review fixes change the delivery (it follows the application's
synchronizations) and add an isspacep.local test to every generic atomic in
probe functions of modules with thread-local maps (lat's histogram add). So
p5 and p6 measure again, on 606e74a5: base, lat, hist on both workloads, and
`lat+sync`, `hist+sync` (BPFTIME_GPU_DELIVERY_AT_SYNC=1: the delivery runs
inside the application's synchronize calls). Rules as above; in addition the
status must show the last synchronization covered (SYNCS_COVERED ==
SYNCS_AT_EXIT) for `+sync` items (rule 1); for the default items it is
reported, not judged. lat and hist must agree with p1-p4 within 2 points,
or the change is reported.

## Correction (2026-11-17, after the runs)

"Items" above gives the largest launch as 445,644 threads (mul_mat_vec_q, tg128): that was the average. The largest launch has 720,896 threads (`gpu/llama-census-20261113`); the 1,048,576 sizing covers it. The rules themselves are unchanged.

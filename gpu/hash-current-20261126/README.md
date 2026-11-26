# Hash maps from GPU probes: the current state (2026-11-26)

User 11-26: start the hash map, measure the current state first. The map-model prototype has only array-shaped maps (1530/1531/1532); this round measures what a GPU probe gets today from a hash map, on dev 70f7bc1f (build `tvf-7d02d677`: dev plus the prototype, whose routing pass does not run on these tools: no item logged it). No fixes this round. Rules written before the passes: `RULES-hash-current.md` (with addenda before s1/s2 and before l3/l4).

## What a GPU hash operation is (read-only survey, `survey.md`)

- **Every lookup, update and delete of a hash map from a GPU probe is a host call.**
  - It goes through one request slot per process, and one CPU watcher thread serves the requests one after another.
  - The lanes of a warp take turns. The GPU itself never touches the map's memory except through the pointer a lookup returns.
  - This holds for `BPF_MAP_TYPE_HASH`. The trampoline has device paths only for array-shaped types.
- **On the Orin, `BPF_MAP_TYPE_GPU_HASH_MAP` (1501) is created as an ordinary HASH** (integrated GPU, no CUDA IPC). Its device implementation (`nv_gpu_shared_hash_map`) runs only on x86 and was never exercised here.
- **A lookup hit returns a device address of the element's value in the shared segment** (registered host memory). The program then reads or adds through it directly.
- **Not measured:** PERCPU_HASH and LRU_HASH from the GPU (described from code only), and what the CPU sees while a kernel runs.
- **How examples use hash maps:** 7 of bpftime's 22 GPU example programs use one; keys are bounded (a constant, pid, SM, warp, block, thread). Three shapes:
  - a counter (lookup, then an atomic add or a `BPF_NOEXIST` insert);
  - a start time per block or thread (update at entry, lookup and delete at exit);
  - a per-launch sum.

## pt_victim (passes h1-h3, s1/s2; `analysis-*.txt`)

Tool `tools/pt_hash.bpf.c`: one operation per build, only today's map types (the hash `h`, side counts in a GPU array 1513 added on the device). Sizes:
- D: `pt_kdiv`, blocks of 256, launched directly, 100,096 threads per launch;
- G: `pt_kflat`, blocks of 48, launched through a CUDA graph, 14,400 threads per launch.

The two differ in kernel, block shape and launch mode, so the gap between their columns (about 1.5 us) is not attributed.

**Per host call, by slope** (`analysis-slope.txt`): the same tool at 4x the size (D4, G8) against D and G. The hash tools start about 350 ms slower than the empty probes, a fixed start-up cost, so dividing by calls alone overstates.

| operation | pt_kdiv (us) | pt_kflat (us) |
|---|---:|---:|
| lookup, hit (key the loader inserted) | 12.68 / 12.76 | 11.06 / 11.20 |
| lookup, miss | 12.04 / 13.37 | 10.46 / 10.50 |
| update an existing key (`BPF_ANY`) | 12.16 / 12.28 | 10.27 / 10.95 |
| delete a missing key | 11.60 / 11.66 | 9.57 / 10.19 |
| delete a present key (by difference: 3 x tid - 2 x tidnd) | about 10.4 | about 10.0 |
| per-thread start time: update, lookup, delete (mean of the 3) | 10.77 / 10.84 | 9.98 / 10.01 |

- Every operation costs about the same, 10-13 us. The cost is the round trip, not the hash table.
- The 11-13 census measured 10.8 us on the old base.
- The miss slope on D differs by 10% between passes (12.04 / 13.37), reported as the rule says.

**Correctness** (three passes, every pass):

| what each thread does | result |
|---|---|
| lookup + atomic add on keys the loader inserted | exact (100,096 and 57,600 counted) |
| start time per thread: update at entry, lookup and delete at exit | exact: every start found, every delete done, 0 entries left; without the delete, entries left = threads per launch |
| the examples' counter idiom, one key per kernel, every thread | loses 2,169-4,570 of 100,096 (D) and 2,275-2,339 of 57,600 (G): 2,170-4,571 and 2,276-2,341 lookups returned NULL before the first insert was served; each of those threads tried a `BPF_NOEXIST` insert and all but one failed. How many depends on how the calls queue in this geometry, so the share is not a property of the idiom |
| the same, key per SM | loses 5,624-5,957 of 100,096 and 2,762-2,902 of 57,600 |
| the same, one thread per block, key per block | exact (391 and 1,200) |
| the same, one thread per block, one key | loses 3-8 of 391, 4-20 of 1,200 |
| the same, one thread per launch | exact |
| lookup, atomic add through the pointer, update `BPF_EXIST` with that pointer (cuda-counter's `total_time_ns`) | loses about half: 48,722-49,552 of 100,096, 28,063-28,497 of 57,600. Consistent with each update moving the value to a new element while other threads still add through the old pointer (the hash map's documented update; not isolated by a run) |

## llama.cpp (passes l1/l2, `analysis-llama.txt`; l3/l4, `analysis-llama-l3l4.txt`)

llama-bench TinyLlama Q4_K_M, `-r 5` with warm-up. CUDA graphs are this build's default.

| probe | workload | base t/s | with the probe, t/s | s per run | host calls per run | per call | overhead |
|---|---|---:|---:|---:|---:|---:|---:|
| rms_norm, every thread: lookup hit + atomic add (the 11-13 census shape) | 32-token prefill | 1376.0 / 1376.1 | 4.04 / 3.93 | 0.0233 -> 7.92-8.15 | 705,536 | 11.20 / 11.52 us | 341-351 times slower |
| rms_norm, one thread per block: the counter idiom | 32-token prefill | 1376.0 / 1376.1 | 760.0 / 775.2 | 0.0233 -> 0.0413-0.0421 | 1,378 | 13.08 / 13.67 us | 43.7-44.8% |

**One call per launch, against its own zero point (l3/l4).** The per-launch tool's rate filter (two index helpers in the exit probe) runs on every thread of the four kernels: 3.1e9 threads per tg128 run on mul_mat_vec_q alone. l1/l2 took it against the run without the agent, so their 26.5-26.8% and "about 44 us per call" were the whole tool, not the host calls. l3/l4 add the same probe without the hash operation (`ldrf`), and the same items with CUDA graphs off:

| tg128, four kernels | graphs on: t/s (l3 / l4) | graphs off: t/s (l3 / l4) |
|---|---:|---:|
| no agent | 68.00 / 67.84 | 62.95 / 62.85 |
| the filter alone, one thread per launch counted on the device | 56.90 / 56.86 (16.2-16.3%) | 53.03 / 53.08 (15.5-15.8%) |
| the filter and the counter idiom on the hash map | 49.71 / 49.65 (26.8-26.9%) | 47.54 / 47.19 (24.5-24.9%) |
| the hash calls alone: s per run over the filter | +0.3256 / +0.3270 | +0.2790 / +0.3008 |
| per call (15,616 per run) | 20.85 / 20.94 us | 17.87 / 19.26 us |

- **About 10 of the 27 points are the hash calls;** about 16 are a probe with a filter on every thread.
- **A sparse call costs about 18-21 us,** against 10-13 back to back, with or without graphs. The watcher is not as fast between sparse calls; the graph launches are not the cause.
- **pp512:** 90 calls per run cost 0.0002-0.0007 s per run over the filter (2.7-7.5 us per call, within the runs' noise).
- **Counts:** exact (78,202 on tg128, 540 on pp512); the filter-only runs leave the hash map empty.

- **Counts:**
  - per thread: exact (4,233,216 = 6 x 705,536);
  - per block: loses 2-4 of 8,268 at first touch;
  - per launch: exact (78,202 on tg128).
- **The census link holds:**
  - then: 7.63 s, 307 times, 10.8 us per call;
  - now: 7.92-8.15 s, 341-351 times, 11.2-11.5 us per call. The base prefill is faster now (23.3 ms against 24.9).
- **Every thread on tg128 (computed, not measured):** mul_mat_vec_q alone runs 3,137,339,392 threads per run. At 11 us per call that is about 34,500 s per run (about 9.6 hours) against 1.88 s.

## What this establishes

- **One host call per operation,** 10-13 us when calls follow each other and about 18-21 us when they are sparse (with or without CUDA graphs). Served one at a time for the whole process: about 80-100 thousand operations per second for the whole GPU.
- **Per-thread hash operations are unusable on real kernels.** A small kernel (rms_norm, 1.5% of a prefill's GPU time) makes a 32-token prefill 341-351 times slower. Per block costs 44% on that kernel. One call per launch on four kernels adds about 10 points on tg128 (0.33 s per run); the probe that picks the launch's first thread costs another 16.
- **Correctness depends on the idiom:**
  - exact with keys inserted beforehand or one writer per key;
  - the examples' first-touch counter loses every increment whose lookup missed before the first insert was served: here 2,169-5,957 of 100,096 per thread; per block, one key: 3-8 of 391, 4-20 of 1,200, 2-4 of 8,268 (rms_norm);
  - lookup-then-update loses about half.
- **The GPU never runs hash code:** the table lives on the CPU side, and the device-side GPU hash map (1501) does not exist on this board.

## Files

- `RULES-hash-current.md`: the rules, with the slope and the per-launch zero-point addenda.
- `survey.md`: the four readers' reports (read only).
- `tools/`:
  - `pt_hash.bpf.c`, `build_hs.sh` (variant names: `hs_<op>_<rate>_<key>_<set>`), `pt_common.h`, `pt_empty.bpf.c`;
  - `pt_loader.c` with `PT_PREFILL` and hash entry counts (`pt_loader-prefill-entries.diff`).
- `scripts/`: `run_hash.sh` (pt_victim), `run_hash_llama.sh`, `analyze_hash.py`, `analyze_hash_llama.py`, `slope_hash.py`.
- `out/{h1,h2,h3,s1,s2,l1,l2,l3,l4}-*`: the runs, without `h.bin`. That is the loader's dump of keys 0-262143, 2 MB each, mostly zeros, left on the board; `maps.json` holds each map's sum and entry count.

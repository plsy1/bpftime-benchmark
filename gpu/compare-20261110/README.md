# Official GPU benchmark on the Orin: official 4708f409 vs dev c115b33d (2026-11-10)

The upstream GPU benchmark (`benchmark/gpu/run_cuda_bench.py`, configs `micro_vec_add_config.json`
and `examples_vec_add_config.json`) run on the Jetson Orin Nano with both builds, next to the
upstream results committed for the RTX 5090 and the Tesla P40.

Full tables: [summary.md](summary.md) (per case: median and range over 3 rounds, ratio to the
case's own baseline, check status, upstream 5090 / P40 numbers).

## Builds

| | tree | build |
|---|---|---|
| official | eunomia-bpf/bpftime 4708f409 | `build-cuda`, CUDA root = symlink tree (`targets/aarch64-linux -> sbsa-linux`), needed to configure on the Orin |
| dev | dev/uprobe-official-4708f4092d1e c115b33d (clean tree, built after the commit) | `build-cuda`, CUDA 13.2 |

Same flags for both: RelWithDebInfo, gcc-13, clang-15 / LLVM 15, LTO off,
`SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG`, `BPFTIME_ENABLE_CUDA_ATTACH=ON`. Library hashes and cache
entries: [build/identity.txt](build/identity.txt). Clocks: jetson_clocks (cpu5 1728000, GPU
1020000000), checked before every batch.

## Run sets

| set | build | micro probe | victim | rounds |
|---|---|---|---|---|
| A | official | copy asking for map types 1512/1513 | legacy launch | 3 (A1, A2, A3) |
| B | dev | copy asking for 1512/1513 | legacy launch | 3 (B1, B2, B3) |
| C | dev | as shipped (1502/1503) | as the framework builds it | 1 |
| D | official | as shipped | as the framework builds it | 1 (a few cases) |
| G | both | gate: threadhist / mem_trace copies on 1512/1513 | legacy | 1 |
| E | both | evidence: every micro mode + 2 examples at debug level, minimal geometry | legacy | 1 |

A and B rounds were interleaved (A1 B1 A2 B2 A3 B3), 10:57-12:07 JST.

Why A/B do not run the benchmark exactly as shipped (D shows what happens then):

- **Victim.** nvcc 13.2 builds `vec_add` against `__cudaLaunchKernel` (CUDA 13 kernel handles).
  Official does not hook it (upstream #646), so its probes never run: in D, CUDA Counter prints no
  calls and takes 46 us instead of 337 ms. A and B use the same source built with
  `-D__NV_LEGACY_LAUNCH`, which calls `cudaLaunchKernel`. C shows dev on the default victim.
- **Map types.** 1502 (per-thread array) and 1503 (GPU array) use CUDA IPC, which the Orin's
  integrated GPU refuses (`cuIpcGetMemHandle` 801). Official cannot create them, and the micro
  probe declares them in every mode, so every micro case fails to load on official (D). 1512/1513
  are the host-memory variants official already supports; the gate (G) shows both builds count
  correctly with them (threadhist 7 threads with counts, mem_trace counter[0]=28 on both). A and B
  use a probe copy with lines 6 and 8 changed to 1512/1513. Dev maps 1502/1503 to 1512/1513 on
  integrated GPUs by itself, so C runs the shipped probe.
- **Skipped cases.** "eBPF Direct Run VecAdd" and "Launch Latency" (not probe overhead) on all
  sets; "Per-GPU-thread array" medium/large/xlarge on official, which writes past its 1024-thread
  map at those geometries.
- **Extra case.** "Baseline (minimal)" in micro, so the CPU-map rows (minimal geometry) have a
  same-geometry baseline; upstream divides them by "Baseline (tiny)".

## Checks

[phase2-verify.csv](phase2-verify.csv) (A-D) and [phase1-verify.csv](phase1-verify.csv) (G, E),
from the per-case log sections:

- Every A-D case: the victim printed `Validation check: C[0] = 0.00, C[1] = 3.00` and its closing
  line (461 rows, all valid and finished).
- Instrumentation: official logs `Recorded pass ... for func _Z9vectorAddPKfS0_Pf`, dev logs
  `loaded the patched image ... (probes on _Z9vectorAddPKfS0_Pf)`; at debug level (E) official
  also logs each launch redirected to the patched kernel (4 per case = 1 warm-up + 3 timed).
  Example loaders' own numbers are in the status column (calls, events, counter[0], threads).
- Failure strings (load, map, launch, fallback) are listed in the status column.
- Dev does not log the patched-image line for the memtrace pass (it is patched when cudart loads
  its module); the gate's mem_trace counter shows it runs. Logging gap only, not fixed here.

## Results (A official vs B dev, medians of 3 rounds)

Baselines agree between the builds (tiny 27.8 / 27.5 us, minimal 39.5 / 39.7 us).

**Host-call cases: dev about 100x faster.**

| case | official | dev | 5090 (upstream) | P40 (upstream) |
|---|---:|---:|---:|---:|
| CPU Array map update (minimal) | 33,713 us | 324 us | 33,450 us | 35,798 us |
| CPU Array map lookup (minimal) | 33,745 us | 367 us | 33,445 us | 35,976 us |
| CPU Hash map update (minimal) | 33,672 us | 323 us | 33,442 us | 36,022 us |
| CPU Hash map lookup (minimal) | 33,708 us | 337 us | 33,537 us | 35,881 us |
| CPU Hash map delete (minimal) | 33,692 us | 319 us | 33,448 us | 36,063 us |
| CUDA Counter (minimal) | 337,373 us | 3,486 us | 337,453 us | 370,793 us |

Official's numbers are the same on the Orin, the 5090 and the P40: the cost is official's host
call path (about 1 ms per call), not the GPU. Both builds count the same CUDA Counter calls (97).

**Device-only probes: same range on both builds.** Empty, entry, exit, entry+exit, global timer,
memtrace and GPU array lookup sit at 1.1-1.5x of baseline on both builds (5.5x for memtrace
large, 2.2-2.9x for array lookup large/xlarge), close to the 5090's 1.2x pattern. Most per-case
differences are a few us inside overlapping ranges. Where the 3-round ranges do not overlap
(medians, dev vs official):

- dev slower by more than 5%: GPU Ringbuf small_64x16 / small / medium (+16% / +19% / +12%, about
  +5-8 us), Entry+Exit small_64x16 / small (+15% / +9%), GPU Array map update large (+13%, 220 vs
  250 us), Memtrace tiny (+6%);
- dev faster by more than 5%: Entry+Exit large (-15%), Per-GPU-thread array tiny / small_64x16
  (-16% / -11%), GPU Array map update small_64x16 / small / xlarge (-17% / -10% / -14%, xlarge
  1,724 vs 1,486 us), GPU Array map lookup medium (-8%), Memtrace large (-6%);
- within 5%: Empty / Entry xlarge, GPU Ringbuf xlarge, GPU Array map lookup xlarge.

Three rounds of one run each is a small sample; the dev slowdowns (ringbuf, entry+exit small,
array update large) are not explained yet.

**Examples.**

| case | official | dev | note |
|---|---:|---:|---|
| Kernel Retsnoop | 61.7 us, 32 events | 46.0 us, 128 events | 32 threads x 4 launches = 128 expected; official loses events |
| Thread Histogram | fails (1502) | 44.7 us, 7 threads | official runs it only with a 1512 copy (gate) |
| Memory Trace | fails (1503) | 56.2 us, counter[0]=28 | official runs it only with a 1513 copy (gate) |

Dev only (official skipped): Per-GPU-thread array medium / large / xlarge 43.9 / 147.4 /
1,165 us (1.6 / 4.7 / 7.3x).

**C (dev, shipped probe, default victim)** matches B within run-to-run spread on every case.

## Upstream tables

The 5090 and P40 results are upstream's committed tables, not re-run here. Three of their rows are
faster than their own baseline (5090 GPU Ringbuf large / xlarge 1.1 us, Per-GPU-thread array
xlarge 1.1 us; P40 the same rows), which suggests those cases did not run the kernel there; they
are shown as published.

## Files

- `summary.md`: all cases, all sets.
- `phase1-verify.csv`, `phase2-verify.csv`: per-case checks.
- `runs/<set>/`: result json/md and runner logs (logs over 1 MB are `.xz`).
- `configs/`: the generated runner configs. `scripts/`: build, config, batch, check and summary
  scripts. `build/`: build logs, loader build logs, victim checks, identity.

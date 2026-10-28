# Entry/exit churn on 1-6 threads: kernel vs bpftime (2026-10-28)

Benchmark 1 of the dimension plan (`../map-usage-20261027`): the dominant real-tool pattern.
A uprobe on `target_func` inserts a start record keyed by the thread (`bpf_map_update_elem`),
the uretprobe looks it up and deletes it; every thread uses its own key in the same hash map
(10240 entries). Kernel `6.8.12-1021-tegra-btf`, `jetson_clocks` (cpu5 1728000 before and after).

## Setup (`src/`)

- `churnloop THREADS SECS [CPUS]`: thread i pinned to CPU 5-i (or the i-th CPU of the list),
  calls the noinline `target_func` in a loop; prints the median calls/s and the exact call count.
- `churn.bpf.c`: `KEY_SIZE` 4 (tid) or 8 (pid_tgid), `VALUE_SIZE` 8 or 112; `CONTROL=1` is the
  same probes and helpers without map operations. Results per thread in `.bss` slots that never
  share a cache line.
- `churn` (loader, bpftime's libbpf 1.7.0): attaches both probes, on SIGINT reads the hits.
  Every run checks hits == the workload's call count and no lookup misses (all 150 runs passed).
- Backends: `none` (workload alone), `kernel`, `09421773` (dev branch, LTO, LLVM 15), `official`
  (177a1dc unpatched); bpftime runs the tool through the syscall server, the workload through
  the agent. 8 s per run, 3 reps, backend order rotated; `run_churn.py`, `summarize.py`.
- Orin Nano CPU topology: two clusters with separate L3, cpu0-3 and cpu4-5. The default placement
  (5,4,3,2,...) keeps 1-2 threads in one cluster and crosses clusters from 4 threads on.

## Results (`run2.jsonl`, `summary-run2.md`; per-call times per thread, medians)

| Threads | kernel total | bpftime total | kernel map | bpftime map | kernel probe w/o map | bpftime probe w/o map |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 2.05 | 0.57 | 0.20 | 0.12 | 1.84 | 0.44 |
| 2 | 3.01 | 0.97 | 0.38 | 0.42 | 2.62 | 0.54 |
| 4 | 5.04 | 9.01 | 0.22 | 6.29 | 4.81 | 2.70 |
| 6 | 6.68 | 15.51 | 0.32 | 10.72 | 6.35 | 4.77 |

(us; 4 B key, 8 B value; "map" = churn minus control: insert + lookup + delete. The 8 B key /
112 B value object and the `official` build are within 10% of these at 2-6 threads; at 1 thread
the dev branch's map cost is 0.12 vs official 0.21.)

Same cluster (`cluster0.jsonl`, threads on cpu0-3):

| Threads | kernel map | bpftime map | kernel total | bpftime total | bpftime probe w/o map |
|---:|---:|---:|---:|---:|---:|
| 2 | 0.30 | 0.41 | 3.00 | 0.96 | 0.54 |
| 4 | 0.21 | 2.68 | 3.36 | 3.60 | 0.91 |

## Findings

1. **bpftime hash maps do not scale with threads.** Every operation, lookups included, takes the
   map's single spinlock (`bpf_map_handler::map_lock`): 0.12 us per call alone, 0.42 with two
   threads, 2.7 with four in one cluster, 6.3 with four across clusters, 10.7 with six. The
   kernel stays at 0.2-0.4 us (lock-free RCU lookups, per-bucket locks, per-CPU free lists). The
   official build behaves the same: the map optimizations changed the 1-thread cost only.
2. **The probe path itself also degrades with threads in bpftime** (no maps: 0.44 -> 2.70 us at 4
   threads across clusters, 0.91 in one cluster): some state shared by all threads on every hit;
   cause not yet known. The kernel's uprobe path degrades too (1.8 -> 6.4 us; 6.8 still has
   global uprobe locking), so bpftime's probe stays cheaper.
3. Net: bpftime is about 3x faster than the kernel with 1-2 threads, about equal with 4 threads
   in one cluster, and 1.8x (4 threads) to 2.3x (6 threads) slower across clusters.

`failed-run1-false-sharing/`: the first matrix, before removing false sharing from the benchmark
(per-thread `.bss` slots sharing lines, one shared `sink` in the workload); it inflated the
control (no-map) costs at 4-6 threads for both backends; map-cost conclusions were the same.
`smoke/`: first smoke run.

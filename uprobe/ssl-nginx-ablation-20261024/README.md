# sslsniff path ablation on db3d475f, fair scope (2026-10-24)

Redo of the July V1-V4 ablation (`summry/archive/bpftime-official-no-btf-ssl-nginx-path-ablation-20260726.md`)
on the current dev branch `db3d475f` (map optimizations, no affinity pinning, fork fix, perf loss fix).

> **Clock note (found 2026-10-24 15:49):** the Jetson rebooted at 05:54 on 10-24 and `jetson_clocks`
> was not re-applied, so both runs in this directory (09:34-10:14 and 15:02-15:31) ran with the
> schedutil governor (0.73-1.73 GHz) instead of a fixed 1.728 GHz. All legs of a run share the
> condition, so the same-run stage comparisons stand, but absolute req/s are lower (baseline 16.3k
> vs 17.2k on 10-23) and noise is higher. Later runs re-apply `jetson_clocks`.

- sslsniff stages from one source with `SSLSNIFF_STAGE` (`sslsniff-src/ablation.diff`, no-BTF build as
  in `../ssl-nginx-2x2-20261023`): 1 = every probe returns at once; 2 = + pid/uid/ktime/comm helpers,
  hash map update/lookup/delete, per-CPU array lookup, event metadata; 3 = + `bpf_probe_read_user`;
  4 = + `bpf_perf_event_output` (full sslsniff; BPF instructions identical to the binary used before).
- Fair scope, same-run: baseline, kernel@1-4 (`sslsniff -p <worker>`), db3d475f@1-4; leg order rotated,
  10 reps, 16 B and 256 KB; wrk -c 100 -d 10. Runner `bench/run_ablation.py`; container removed after.
- Stage cost = value(stage n) - value(stage n-1) per rep, relative to that rep's baseline
  (`bench/analyze.py` -> `bench/summary.md`); 95% t CI.

## 16 B (baseline 61.2 us/request)

| Stage | kernel pp | bpftime pp | bpftime - kernel | kernel us/req | bpftime us/req |
|---|---:|---:|---:|---:|---:|
| 1 probe trigger/dispatch | 15.3 | 9.3 | **-6.1 +/- 1.6** | 11.1 | 6.2 |
| 2 helpers/maps/metadata | 6.3 | 12.9 | **+6.6 +/- 2.3** | 5.8 | 11.1 |
| 3 bpf_probe_read_user | 1.5 | 1.5 | +0.1 +/- 3.4 | 1.5 | 1.7 |
| 4 perf output | 7.6 | 3.5 | **-4.2 +/- 2.6** | 8.7 | 3.6 |
| total | 30.7 | 27.1 | -3.6 | 27.0 | 22.7 |

## 256 KB (baseline 586 us/request)

| Stage | kernel pp | bpftime pp | bpftime - kernel | kernel us/req | bpftime us/req |
|---|---:|---:|---:|---:|---:|
| 1 probe trigger/dispatch | 15.2 | 6.4 | **-8.8 +/- 2.3** | 106 | 41 |
| 2 helpers/maps/metadata | 5.7 | 10.4 | **+4.7 +/- 3.0** | 51 | 79 |
| 3 bpf_probe_read_user | -0.1 | 2.1 | +2.2 +/- 5.0 | -2 | 18 |
| 4 perf output | 11.1 | 7.3 | **-3.8 +/- 1.8** | 121 | 71 |
| total | 31.9 | 26.2 | -5.7 | 275 | 209 |

## Findings

1. Stage 2 (helpers, maps, metadata) is now bpftime's largest cost and the only stage where it is
   clearly worse than kernel: 11.1 us/request at 16 B, twice the kernel's 5.8 us. In July it was
   at parity (+1.0 pp); perf output was the worst stage then (+8.4 pp vs kernel).
2. Perf output is now cheaper than kernel (-4 pp), after removing affinity pinning and with the
   loss fix. Probe dispatch keeps its structural advantage (-6 to -9 pp).
3. `bpf_probe_read_user` costs only 1.5-2 pp, the same order as kernel; not the main gap. All builds
   here use `ENABLE_PROBE_READ_CHECK=OFF` (checked in each `CMakeCache.txt`), so it is a plain
   `memcpy`; the `process_vm_readv` path added upstream in f311de23 (2026-08-13) is only used with
   the check ON, which is the CMake default but not the benchmark Makefile target's setting.
4. Every stage costs far more per hit than the microbenchmarks suggest: stage 1 is 6.2 us per
   16 B request for 4 probe hits (~1.6 us/hit) against ~160 ns/hit in the uprobe microbenchmark;
   kernel stage 2 (~5.8 us for ~14 map operations and a few helpers) inflates similarly. Inside
   nginx the probe and helper code runs cold.
5. In stage 2 the bpftime helpers include system calls the kernel does not need: `bpf_get_current_uid_gid`
   calls `getgid()` and `getuid()` on every call (4 calls, 8 syscalls per 16 B request), and the
   per-CPU array lookup asks for the current CPU. `get_current_comm` and `pid_tgid` are cached and
   `ktime_get_ns` is a vDSO read. Splitting stage 2 further would show which of these dominates.

## Comparison with published bpftime numbers

A literature check (OSDI '25 "Extending Applications Safely and Efficiently", arXiv 2311.07923,
LPC 2023 slides, later GPU/NCCL/uXDP papers) found no ARM results anywhere. Published sslsniff + nginx:

| Source | Machine | Load | kernel vs native | bpftime vs native |
|---|---|---|---:|---:|
| LPC 2023 slides | x86, Linux 6.2 (i7-11800H per repo README) | wrk -c 100, ~13 B page | -57.98% | -12.35% |
| OSDI '25 Fig. 9 | 2 x Xeon E5-2697 v2 (24 cores / 48 threads) | wrk 4 threads, 512 connections, 1-256 KB | up to -28.06% | at most -7.41% |
| this run (db3d475f) | Jetson Orin, 6 x A78AE 1.728 GHz | wrk -c 100, 16 B / 256 KB | -30.7% / -31.9% | -27.1% / -26.2% |

Notes: the paper does not state nginx worker count, kernel version or build options; the repo
README of that era says the sslsniff consumer was modified not to print and bpftime was built
with LTO and lock-free maps. The microbenchmark table of the paper (Table 3) matches a 2025-04-30
run on a 4-core Core Ultra 7 258V laptop in the repo; its "user memory read 1.5 ns" is a 5-byte
read amortized over a 1000-iteration loop with checks off. Kernel cost here is similar to the
paper's; the gap is in bpftime's per-event cost on this machine, mainly stage 2.

## Stage 2 split (leave-one-out, 16 B, 2026-10-24 15:02-15:31 JST)

Stage 2 rebuilt with switches that each remove one group and use constants instead
(`stage2-split/src/stage2-split.diff`; the plain stage 2 build has identical BPF instructions to
`stage2`). Without the per-CPU array lookup the event is assembled in a global `.bss` byte array.
Same-run fair scope, 13 legs (baseline, kernel and db3d475f at stage 1, stage 2 and the four
variants), 10 reps; `bench/analyze_split.py` -> `bench/summary-split.md`. Cost of a group =
stage 2 minus stage 2 without it, per rep.

| Part | kernel us/req | bpftime us/req | bpftime - kernel |
|---|---:|---:|---:|
| stage 2 total | 7.3 +/- 0.9 | 9.9 +/- 1.3 | +2.6 +/- 1.0 |
| `bpf_get_current_uid_gid` | 1.1 +/- 1.5 | **5.0 +/- 1.6** | **+3.8 +/- 1.9** |
| pid_tgid + ktime_get_ns + get_current_comm | 1.6 +/- 1.1 | 1.9 +/- 1.2 | +0.3 +/- 1.7 |
| hash map update/lookup/delete | 5.0 +/- 1.3 | 2.2 +/- 2.0 | -2.7 +/- 2.1 |
| per-CPU array lookup | 1.1 +/- 1.2 | -1.2 +/- 3.9 | -2.3 +/- 4.4 |

`bpf_get_current_uid_gid` is half of bpftime's stage 2 and more than its whole disadvantage against
kernel: it calls `getgid()` and `getuid()` on every invocation, 4 invocations (8 system calls) per
16 B request, about 0.6 us per system call here. bpftime's hash map operations are cheaper than the
kernel's in this program; the per-CPU array lookup is within noise; pid/ktime/comm are on par.

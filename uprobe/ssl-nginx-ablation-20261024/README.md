# sslsniff path ablation on db3d475f, fair scope (2026-10-24)

Redo of the July V1-V4 ablation (`summry/archive/bpftime-official-no-btf-ssl-nginx-path-ablation-20260726.md`)
on the current dev branch `db3d475f` (map optimizations, no affinity pinning, fork fix, perf loss fix).

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
3. `bpf_probe_read_user` (now a `process_vm_readv` syscall since upstream f311de23) costs only
   1.5-2 pp, the same order as kernel; not the main gap.
4. Every stage costs far more per hit than the microbenchmarks suggest: stage 1 is 6.2 us per
   16 B request for 4 probe hits (~1.6 us/hit) against ~160 ns/hit in the uprobe microbenchmark;
   kernel stage 2 (~5.8 us for ~14 map operations and a few helpers) inflates similarly. Inside
   nginx the probe and helper code runs cold.
5. In stage 2 the bpftime helpers include system calls the kernel does not need: `bpf_get_current_uid_gid`
   calls `getgid()` and `getuid()` on every call (4 calls, 8 syscalls per 16 B request), and the
   per-CPU array lookup asks for the current CPU. `get_current_comm` and `pid_tgid` are cached and
   `ktime_get_ns` is a vDSO read. Splitting stage 2 further would show which of these dominates.

# Path ablation on 7ed339ad, stage 1 split and perf profile (2026-10-24)

Clocks fixed at 1.728 GHz (`jetson_clocks`), unlike `../ssl-nginx-ablation-20261024`. Same stage
binaries (`sslsniff/stage1-4`) plus `stage1u` (empty program, entry uprobes only) and `stage1r`
(empty program, uretprobes only; `src/stage1-split-sslsniff.c.diff`). Fair scope, same run,
10 reps; 16 B with 13 legs, 256 KB with 9 legs (`bench/summary.md`). Container removed after.

## Four stages, 16 B (baseline 58.0 us/request), us per request

| Stage | kernel | 7ed339ad | bpftime - kernel (pp) |
|---|---:|---:|---:|
| 1 probe trigger/dispatch | 10.6 | 5.6 | -6.6 +/- 2.0 |
| 2 helpers/maps/metadata | 4.7 | 6.2 | +2.4 +/- 4.2 (parity after the uid_gid cache) |
| 3 bpf_probe_read_user | 0.9 | -0.7 | noise |
| 4 perf output | 10.5 | 4.6 | -4.5 +/- 1.8 |
| total | 26.7 | 15.7 | -10.3 |

256 KB: total 275 (kernel) vs 216 us/request (7ed339ad); stage CIs are wide at this payload.

## Stage 1 split, 16 B, us per request over baseline

| Probes | kernel | 7ed339ad |
|---|---:|---:|
| entry uprobes only (2 hits) | 6.3 | 4.1 |
| uretprobes only (2 hits) | 7.7 | 4.9 |
| both (4 hits) | 10.6 | 5.6 |

With empty programs most of bpftime's cost is intercepting the function at all (Frida's trampoline
and interceptor); a second listener on the same function adds little.

## perf (`perf/`, perf 6.8.12 from linux-tools-6.8.0-1020-nvidia-tegra, nginx worker, 8 s)

- `perf stat` per request (`perf/stat-*.txt`): IPC stays 0.76 with empty probes; the cost is extra
  instructions (+4.7-6.2k per request, about 1.2-1.5k per probe hit), not cache or TLB misses.
  Microbenchmarks run the same path at a much higher IPC, which is why they look ten times cheaper.
- Full sslsniff (`perf/stage4-7ed339ad.data`, `perf/leafs.py`): `bpf_perf_event_output` was 4.67%
  of worker samples, 2.8% of them the `getpid` and `gettid` system calls -> fixed in `bca2ebce`
  (`../perf-output-identity-20261024`); after it 1.58%.
- After bca2ebce (`perf/stage4-bca2ebce.data`): agent code 7.4%, JIT 1.0%, vDSO 0.6% of worker
  cycles (about 6.7 us/request), spread over Frida's interceptor, hash map operations,
  clock_gettime, the generic perf output lookups and program dispatch; no single item dominates.

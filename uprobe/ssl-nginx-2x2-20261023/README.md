# ssl-nginx: does the map optimization show up end to end? (2026-10-23)

Same-run, fair-scope comparison on Jetson Orin: nginx 1.24 (one worker), OpenSSL 3.0.13,
wrk `-c 100 -d 10` over HTTPS, traced by sslsniff. Every leg restarts nginx and the tracer;
leg order rotates each rep; 10 reps per round, 2 rounds per payload. Kernel sslsniff traces
only the nginx worker (`-p <pid>`), as bpftime does. Runner: `bench/run_fair.py`; tables:
`bench/summarize.py` -> `bench/summary.md`.

## Legs

| Leg | Build |
|---|---|
| baseline | no tracer |
| kernel | kernel eBPF sslsniff (no-BTF build, `sslsniff/`) |
| official | official 177a1dc, unchanged |
| official-noaff | official 177a1dc + only the removal of per-event CPU affinity pinning in `bpf_perf_event_output` (`no-affinity.patch`) |
| opt-noaff-fork | dev branch `a90fa447`: all map optimizations + affinity removal + fork fix |

`opt-aff` (72aebadd) and `opt-noaff` (20870a03) are kept in `builds/` but were dropped: they
record no events (see below).

## Finding 1: the array JIT broke tracing in forked children (fixed)

The first audit (`bench/audit-16.json`) gave 2.0 events/request for kernel, official and
official-noaff, and **0** for 72aebadd and 20870a03. Since the array lookup JIT became the
default (`c0939bd`, 10/21), a program bound to arrays returned ENOTSUP in fork children;
sslsniff has a per-CPU array, and nginx attaches in the master and serves in the forked
worker. `a90fa447` makes `fork()` take one shared map reference per binding for the child,
so the child runs the inherited program. Audit after the fix (`bench/audit-16-fork.json`):
kernel 2.000, official-noaff 2.000, opt-noaff-fork 2.000 events/request.

## Finding 2: throughput (median req/s, 20 samples per leg)

| Leg | 16 B | vs kernel | 256 KB | vs kernel |
|---|---:|---:|---:|---:|
| baseline | 17,241 | +43.3% | 1,752.7 | +44.1% |
| kernel | 12,027 | 0 | 1,216.4 | 0 |
| official | 10,586 | -12.0% | 1,123.9 | -7.6% |
| official-noaff | 12,869 | +7.0% | 1,342.5 | +10.4% |
| opt-noaff-fork | 12,895 | +7.2% | 1,348.1 | +10.8% |

CV is 0.7-1.7% per leg; the two rounds agree within 0.6%.

- **Affinity removal: +21.6% (16 B) / +19.4% (256 KB)** over official. This is the July fix,
  reproduced on 177a1dc.
- **Map optimizations: +0.2% / +0.4%**, inside the noise. sslsniff does a few map operations per
  event; bpftime adds about 10 us per event here (77.6 vs 58.0 us per request at 16 B, two events),
  so saving tens of ns per map operation is below 1% of the tracing cost. The cost is in the
  uprobe entry/exit and the perf output path, not in map operations.

## Finding 3: bpftime silently loses large perf events (all builds)

With 256 KB responses (`bench/audit-256k-fork.json`) the kernel records 17.0 SSL_write
events/request (16 KB TLS records); every bpftime build records about 8 (official 8.6,
official-noaff 8.2, opt-noaff-fork 8.5). Each event is about 8 KB; sslsniff's 16-page
(64 KB) per-CPU buffer overflows within one response. bpftime's software perf buffer drops a
record that does not fit (`append_record_parts` returns false, `output_data` still returns 0)
and emits no PERF_RECORD_LOST, so the consumer never learns about the loss. The 256 KB
throughput above is therefore not like for like: bpftime delivers about half the data.
16 B is unaffected (2.0 events/request on all legs).

## Follow-up: Finding 3 fixed by db3d475f (2026-10-23 evening)

Cause, builds, audits and the fix session's short runs: `README-perf-lost.md`. Summary: bpftime's
emulated `epoll_wait` polled every 1 ms, so a 64 KB per-thread shard overflowed within one 256 KB
response. `db3d475f` reports drops as `PERF_RECORD_LOST` / `-ENOSPC` and wakes the consumer once a
shard is half full. Audit at 256 KB: 17.85-18.04 events/request delivered (kernel 17.90-17.98),
losses reported at the kernel's level; 16 B unchanged (2.0, no loss).

Same-run fair comparison, 2 rounds x 10 reps (`bench/fair-db-*.json`, `bench/summarize_db.py` ->
`bench/summary-db3d475f.md`; build `builds/db3d475f`):

| Leg | 16 B | vs kernel | 256 KB | vs kernel |
|---|---:|---:|---:|---:|
| baseline | 17,284 | +44.4% | 1,741 | +44.6% |
| kernel | 11,967 | 0 | 1,204 | 0 |
| official | 10,617 | -11.3% | 1,124 | -6.7% |
| a90fa447 (before) | 12,903 | +7.8% | 1,334 | +10.7% (delivers about half the events) |
| db3d475f | 12,837 | +7.3% | 1,285 | **+6.7% (all events)** |

16 B: -0.5% vs a90fa447, within noise (the half-full wakeup never fires there). 256 KB: -3.7%, the
cost of delivering the other half of the events. The fair 256 KB result is bpftime +6.7% over kernel.

## Payload sweep (official draw_figture.py sizes, fair scope)

`bench/run_sweep.sh`: the 7 sizes of the official `benchmark/ssl-nginx/draw_figture.py`, each with
`run_fair.py` (5 reps, leg order rotated); legs baseline, kernel, official 177a1dc, db3d475f.
Figures: `bench/absolute_performance.png` (official layout, median with min-max whiskers) and
`bench/relative_to_kernel.png` (paired per-rep ratio to kernel; median, min-max band).
`bench/plot_sweep.py` -> `bench/summary-sweep.md`. Run 18:49-19:19 JST.

| Leg | 16b | 1kb | 2kb | 4kb | 16kb | 128kb | 256kb |
|---|---:|---:|---:|---:|---:|---:|---:|
| Baseline (no tracing) req/s | 17188 | 16541 | 15889 | 14859 | 10104 | 3146 | 1745 |
| Kernel sslsniff req/s | 12000 | 11495 | 11102 | 10495 | 7255 | 2227 | 1218 |
| bpftime official 177a1dc req/s | 10577 | 10316 | 9943 | 9666 | 6480 | 2011 | 1121 |
| bpftime db3d475f req/s | 12879 | 12389 | 11957 | 11207 | 7773 | 2327 | 1263 |
| Baseline (no tracing) vs kernel | +44.0% | +44.0% | +43.3% | +41.6% | +39.1% | +42.0% | +44.9% |
| bpftime official 177a1dc vs kernel | -11.8% | -10.3% | -10.4% | -7.3% | -11.5% | -9.2% | -8.1% |
| bpftime db3d475f vs kernel | +7.3% | +7.6% | +7.4% | +5.9% | +7.2% | +5.0% | +3.4% |

db3d475f is ahead of kernel at every size (+3.4% to +7.6%); official 177a1dc is behind at every
size (-7.3% to -11.8%). The lead narrows at 128-256 KB, where each request carries 9-17 perf events
of about 8 KB.

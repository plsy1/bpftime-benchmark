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

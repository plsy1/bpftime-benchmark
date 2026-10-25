# Real-tool throughput: no tracing / kernel / official / bca2ebce (2026-10-25)

Tools that passed the functional audit (`../realprog-audit-20261025`) and have a throughput
workload. Each run: start the tool, run the workload for 10 s, record the workload's median
ops/s over its one-second intervals and the tool's event count. 5 rounds, backend order rotated
per round. `jetson_clocks` applied (cpu5 1728000), machine otherwise idle, stock kernel
6.8.12-1021-tegra.

- `none`: workload alone (same `sudo env`, no preload, no tool).
- `kernel`: kernel eBPF. `official`: upstream 177a1dc unpatched, CPU affinity pinning included
  (`ssl-nginx-2x2-20261023/builds/official`; corrected 2026-10-25, an earlier version of this
  README named `official-noaff`). `bca2ebce`: dev branch head. The bca2ebce vs official gap
  therefore includes the removal of affinity pinning (20870a03).

Harness: `../realprog-audit-20261025/src/harness/measure.py` (uses `audit.py`).
Command: `python3 measure.py --out measure.jsonl` (defaults: 3 tools, 4 backends, 5 rounds, 10 s).

## Result (median of 5, ops/s of the traced workload)

| Tool (workload) | none | kernel | official | bca2ebce | bca2ebce vs kernel | bca2ebce vs official |
|---|---|---|---|---|---|---|
| gethostlatency (getaddrinfo "localhost" loop; uprobe + uretprobe, perf buffer) | 112,553 | 59,552 (-47.1%) | 55,252 (-50.9%) | 88,590 (-21.3%) | 1.49x | +60.3% |
| funclatency, bpftime example (empty noinline function; uprobe + uretprobe, histogram map) | 89,767,866 | 507,406 | 1,678,052 | 1,881,506 | 3.71x | +12.1% |
| malloc example (malloc/free loop; uprobe on malloc, hash map, bpf_printk per call) | 42,384,201 | 535,180 | 1,921,304 | 2,142,284 | 4.00x | +11.5% |

Spread (max-min over 5 runs) is under 5% of the median in every cell.

Event counts (reported / expected):

- gethostlatency: kernel 0.998-1.005, official 0.997-1.000, bca2ebce 0.991-0.995. No
  "lost events" lines from the tool; the bca2ebce shortfall is probably events not yet drained
  when the tool is stopped (not verified).
- funclatency: 0.9996-1.0005 on every backend.
- malloc example: kernel and bca2ebce 0.999-1.001; official 1.20, 1.10, 1.00, 1.00, 1.10. The
  excess is the hash map dropping `BPF_EXIST` (audit README, bug 3); the code path is the same
  in bca2ebce, it just did not hit in these runs.

## Caveats

- funclatency and malloc trace a function that costs a few ns, so the numbers are the probe
  cost itself (about 2 us per call on kernel, 0.5 us on bca2ebce), not a realistic slowdown.
- The malloc example calls `bpf_printk` on every malloc. The kernel writes it into the trace
  ring; bpftime writes a line to the traced process's stdout (here a pipe drained in 1 MB
  chunks). That difference is part of the number.
- The kernel's malloc uprobe is attached to libc for every process; only the workload's
  throughput is measured.

## Files

`measure.jsonl` (one line per run), `measure.log`, `measure/<tool>.<backend>.r<N>.{workload,tool}.txt`
(tool output over 2 MB trimmed to head and tail; printk floods counted, not kept).

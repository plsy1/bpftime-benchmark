# Fair payload sweep with bca2ebce (2026-10-24 20:31-21:01 JST)

The 7 sizes of the official `benchmark/ssl-nginx/draw_figture.py`, fair scope (`bench/run_fair.py`,
kernel sslsniff `-p <worker>`), 5 reps per size, legs baseline / kernel / official 177a1dc /
bca2ebce (current dev branch), clocks fixed at 1.728 GHz. `bench/run_sweep.sh`,
`bench/plot_sweep.py` -> `bench/summary-sweep.md`, figures `bench/absolute_performance.png`,
`bench/relative_to_kernel.png`, `bench/overhead_trend.png`. Events per request from 3 s audits at
each size (`bench/audit-<size>.json`; kernel and bca2ebce agree). Containers removed after.

| | 16b | 1kb | 2kb | 4kb | 16kb | 128kb | 256kb |
|---|---:|---:|---:|---:|---:|---:|---:|
| events/request | 2 | 2 | 2 | 2 | 3 | 10 | 18 |
| kernel loss vs no tracing | 29.9% | 29.6% | 30.0% | 29.2% | 27.6% | 30.3% | 29.1% |
| official loss vs no tracing | 40.0% | 37.5% | 37.3% | 35.7% | 34.7% | 35.4% | 36.7% |
| bca2ebce loss vs no tracing | 17.9% | 19.0% | 20.5% | 19.6% | 17.4% | 22.7% | 21.0% |
| bca2ebce vs kernel | +16.7% | +15.9% | +13.8% | +14.9% | +14.1% | +11.3% | +12.2% |
| kernel us per SSL event | 12.4 | 12.8 | 13.7 | 14.0 | 12.6 | 13.9 | 13.1 |
| official us per SSL event | 19.3 | 18.1 | 18.8 | 18.6 | 17.5 | 17.6 | 18.5 |
| bca2ebce us per SSL event | 6.4 | 7.1 | 8.2 | 8.2 | 7.0 | 9.3 | 8.5 |

## Trend over payload size

1. The loss against no tracing is roughly flat for every tracer (kernel 28-30%, bca2ebce 17-23%,
   official 35-40%). Both the work per request (one TLS record per 16 KB) and the number of SSL
   events grow with the response, so the ratio barely moves.
2. Per SSL event, the kernel costs about 13 us at every size: its cost is the uprobe trap and the
   helpers, not the data. bca2ebce costs 6.4 us for 16 B events and 8-9 us once an event carries
   2-8 KB (sslsniff copies up to 8 KB per event: `bpf_probe_read_user` plus the perf output copy).
   The size-dependent part, about 2-3 us per large event, is copying.
3. So bpftime's lead over kernel narrows with payload size, from +16-17% (16 B-1 KB) to +11-12%
   (128-256 KB), while official 177a1dc trails kernel by 8-14% at every size.

5 reps per size; the min-max bands in `relative_to_kernel.png` are a few points wide, so
differences of 1-2 points between neighbouring sizes are within noise.

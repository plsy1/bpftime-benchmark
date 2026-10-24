# Ordinary hash update regression recheck

12 balanced A/B/C blocks (36 fresh processes/maps), original full-suite BPF object and victim, 10,000 invocations per process. Each invocation performs 1,000 map operations. All 17 cases were retained in each process. CPU 5 locked at 1,728 MHz; frozen binaries checked before/after. No source or runtime binary changed.

| Arm | Mean ns/op | Median ns/op | Mean vs previous |
|---|---:|---:|---:|
| previous-on | 34.7888 | 35.7980 | — |
| current-on | 34.7788 | 35.8100 | -0.03% |
| current-off | 35.0033 | 34.6180 | +0.62% |

All samples, including the >40 ns current-off sample, are retained in means and bootstrap calculations. Fast/slow bands are descriptive only, not exclusions.

| Arm | <34.5 ns count / mean | 34.5–38 ns count / mean | >38 ns count |
|---|---:|---:|---:|
| previous-on | 5 / 33.3047 | 7 / 35.8489 | 0 |
| current-on | 5 / 33.2799 | 7 / 35.8494 | 0 |
| current-off | 6 / 33.3070 | 5 / 35.8612 | 1 |

## Interpretation

The prior +3.9% full-run difference was not reproduced as a stable version slowdown; both binaries and both flag settings show the same fast/slow regimes. Exact source of inter-process variation remains unproven.

Address-normalized instructions match for both the 64-instruction public hash-update helper and the 192-instruction lower-level hash-table update routine. This does not prove identical JIT placement, cache behavior, memory layout or scheduling. Those mechanisms have not been isolated.

The current-on mean-change paired bootstrap interval is [-2.8632556871802883, 2.8817894960835977] percent. This is an estimate under the independent-block assumption, not proof against every small regression.

Do not replace the historical full-ten-round table with these recheck numbers or recalculate a kernel ratio: this recheck did not rerun kernel. Its purpose is comparing the two frozen versions under the same original workload.

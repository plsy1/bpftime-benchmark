# Full ten-round regression comparison

Raw average ns/invocation divided by 1,000 operations. Lower is better.

Both array lookup results are excluded from historical gain claims: the old workload does not consume the returned value. Even where CPU-query calls remain, its pointer/value work can simplify after inlining. Use the independent consuming workloads below.

| Operation | Status | Initial ns | Initial/kernel | Previous ns | Current ns | Same-run kernel ns | Current/kernel | vs. initial | vs. previous |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | JIT (separate consuming test) | 15.94 | 4.22x | 1.17 | Not comparable | 3.81 | Separate test | — | — |
| Ordinary array update | Optimized (C++) | 23.51 | 1.78x | 11.55 | 11.59 | 13.10 | 0.88x | -50.7% | +0.3% |
| Ordinary hash lookup | Optimized (C++) | 58.47 | 2.04x | 26.98 | 27.07 | 29.33 | 0.92x | -53.7% | +0.4% |
| Ordinary hash update | Already faster; shared-path gains | 63.18 | 0.67x | 34.30 | 35.63 | 94.30 | 0.38x | -43.6% | +3.9% |
| Ordinary hash delete | Already faster; shared-path gains | 42.64 | 0.40x | 37.17 | 36.94 | 107.70 | 0.34x | -13.4% | -0.6% |
| Per-CPU array lookup | JIT (separate consuming test) | 27.73 | 6.51x | 10.88 | Not comparable | 4.43 | Separate test | — | — |
| Per-CPU array update | Optimized (C++) | 75.93 | 4.55x | 14.52 | 14.52 | 16.57 | 0.88x | -80.9% | -0.0% |
| Per-CPU hash lookup | Optimized (C++) | 141.25 | 4.97x | 27.72 | 27.67 | 29.33 | 0.94x | -80.4% | -0.2% |
| Per-CPU hash update | Optimized (C++) | 220.96 | 3.26x | 44.44 | 44.52 | 67.66 | 0.66x | -79.9% | +0.2% |
| Per-CPU hash delete | Optimized (C++) | 986.68 | 6.50x | 99.86 | 100.18 | 151.67 | 0.66x | -89.8% | +0.3% |

## Consuming per-cpu array lookup (6 paired rounds)

Runtime keys, actual value reads and checked checksum; raw ns/element includes probe/key generation/value reads/result accounting. Not comparable to the historical pure map-operation table.

| Case | Inline off | Inline on | Kernel | Inline/kernel | vs. off |
|---|---:|---:|---:|---:|---:|
| hit | 12.29 | 6.65 | 6.83 | 0.97x | -45.9% |
| mixed | 20.28 | 12.34 | 12.81 | 0.96x | -39.1% |

## Consuming ordinary array lookup (3 paired rounds)

Runtime keys, actual value reads and checked checksum; raw ns/element includes probe/key generation/value reads/result accounting. Not comparable to the historical pure map-operation table.

| Case | Inline off | Inline on | Kernel | Inline/kernel | vs. off |
|---|---:|---:|---:|---:|---:|
| hit | 7.90 | 3.10 | 5.78 | 0.54x | -60.8% |
| mixed | 17.33 | 9.68 | 12.20 | 0.79x | -44.1% |

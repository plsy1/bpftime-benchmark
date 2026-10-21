# Full ten-round regression comparison

Raw average ns/invocation divided by 1,000 operations. Lower is better.

Both array lookup results are excluded from historical gain claims: the old workload does not consume the returned value. Even where CPU-query calls remain, its pointer/value work can simplify after inlining. Use the independent consuming workloads below.

| Operation | Status | Initial ns | Initial/kernel | Previous ns | Current ns | Same-run kernel ns | Current/kernel | vs. initial | vs. previous |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | JIT (separate consuming test) | 15.94 | 4.22x | 1.17 | Not comparable | 3.74 | Separate test | — | — |
| Ordinary array update | Optimized (C++) | 23.51 | 1.78x | 11.53 | 11.55 | 13.04 | 0.89x | -50.9% | +0.2% |
| Ordinary hash lookup | Optimized (C++) | 58.47 | 2.04x | 27.19 | 26.90 | 29.00 | 0.93x | -54.0% | -1.1% |
| Ordinary hash update | Already faster; shared-path gains | 63.18 | 0.67x | 35.07 | 34.21 | 94.95 | 0.36x | -45.9% | -2.5% |
| Ordinary hash delete | Already faster; shared-path gains | 42.64 | 0.40x | 43.69 | 36.64 | 108.06 | 0.34x | -14.1% | -16.1% |
| Per-CPU array lookup | JIT (separate consuming test) | 27.73 | 6.51x | 4.11 | Not comparable | 4.41 | Separate test | — | — |
| Per-CPU array update | Optimized (C++) | 75.93 | 4.55x | 14.39 | 14.41 | 16.58 | 0.87x | -81.0% | +0.1% |
| Per-CPU hash lookup | Optimized (C++) | 141.25 | 4.97x | 27.74 | 27.74 | 29.37 | 0.94x | -80.4% | -0.0% |
| Per-CPU hash update | Optimized (C++) | 220.96 | 3.26x | 44.50 | 44.42 | 68.11 | 0.65x | -79.9% | -0.2% |
| Per-CPU hash delete | Optimized (C++) | 986.68 | 6.50x | 99.81 | 98.77 | 149.54 | 0.66x | -90.0% | -1.0% |

## Consuming per-cpu array lookup (6 paired rounds)

Runtime keys, actual value reads and checked checksum; raw ns/element includes probe/key generation/value reads/result accounting. Not comparable to the historical pure map-operation table.

| Case | Pre-default good INLINE | Current INLINE | Kernel | Current/kernel | vs. pre-default good |
|---|---:|---:|---:|---:|---:|
| hit | 5.19 | 5.12 | 6.83 | 0.75x | -1.3% |
| mixed | 11.54 | 10.64 | 12.75 | 0.83x | -7.8% |

## Consuming ordinary array lookup (6 paired rounds)

Runtime keys, actual value reads and checked checksum; raw ns/element includes probe/key generation/value reads/result accounting. Not comparable to the historical pure map-operation table.

| Case | Pre-default good INLINE | Current INLINE | Kernel | Current/kernel | vs. pre-default good |
|---|---:|---:|---:|---:|---:|
| hit | 3.10 | 3.09 | 5.88 | 0.53x | -0.0% |
| mixed | 9.67 | 8.45 | 12.31 | 0.69x | -12.7% |

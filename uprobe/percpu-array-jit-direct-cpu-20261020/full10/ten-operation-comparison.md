# Full ten-round regression comparison

Raw average ns/invocation divided by 1,000 operations. Lower is better.

Both array lookup results are excluded from historical gain claims: the old workload does not consume the returned value. Even where CPU-query calls remain, its pointer/value work can simplify after inlining. Use the independent consuming workloads below.

| Operation | Status | Initial ns | Initial/kernel | Previous ns | Current ns | Same-run kernel ns | Current/kernel | vs. initial | vs. previous |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | JIT (separate consuming test) | 15.94 | 4.22x | 1.18 | Not comparable | 3.79 | Separate test | — | — |
| Ordinary array update | Optimized (C++) | 23.51 | 1.78x | 11.59 | 11.62 | 13.02 | 0.89x | -50.6% | +0.3% |
| Ordinary hash lookup | Optimized (C++) | 58.47 | 2.04x | 27.07 | 26.97 | 28.40 | 0.95x | -53.9% | -0.4% |
| Ordinary hash update | Already faster; shared-path gains | 63.18 | 0.67x | 35.63 | 34.54 | 94.45 | 0.37x | -45.3% | -3.1% |
| Ordinary hash delete | Already faster; shared-path gains | 42.64 | 0.40x | 36.94 | 37.06 | 108.05 | 0.34x | -13.1% | +0.3% |
| Per-CPU array lookup | JIT (separate consuming test) | 27.73 | 6.51x | 5.81 | Not comparable | 4.40 | Separate test | — | — |
| Per-CPU array update | Optimized (C++) | 75.93 | 4.55x | 14.52 | 14.55 | 16.60 | 0.88x | -80.8% | +0.2% |
| Per-CPU hash lookup | Optimized (C++) | 141.25 | 4.97x | 27.67 | 27.70 | 29.53 | 0.94x | -80.4% | +0.1% |
| Per-CPU hash update | Optimized (C++) | 220.96 | 3.26x | 44.52 | 44.45 | 67.95 | 0.65x | -79.9% | -0.2% |
| Per-CPU hash delete | Optimized (C++) | 986.68 | 6.50x | 100.18 | 99.89 | 149.86 | 0.67x | -89.9% | -0.3% |

## Consuming per-cpu array lookup (6 paired rounds)

Runtime keys, actual value reads and checked checksum; raw ns/element includes probe/key generation/value reads/result accounting. Not comparable to the historical pure map-operation table.

| Case | Previous INLINE | Current INLINE | Kernel | Current/kernel | vs. previous INLINE |
|---|---:|---:|---:|---:|---:|
| hit | 6.65 | 5.15 | 6.83 | 0.75x | -22.5% |
| mixed | 12.48 | 11.51 | 12.72 | 0.90x | -7.8% |

## Consuming ordinary array lookup (3 paired rounds)

Runtime keys, actual value reads and checked checksum; raw ns/element includes probe/key generation/value reads/result accounting. Not comparable to the historical pure map-operation table.

| Case | Previous INLINE | Current INLINE | Kernel | Current/kernel | vs. previous INLINE |
|---|---:|---:|---:|---:|---:|
| hit | 3.09 | 3.09 | 5.88 | 0.53x | +0.0% |
| mixed | 9.69 | 9.65 | 12.32 | 0.78x | -0.4% |

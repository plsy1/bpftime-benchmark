# Full ten-round regression comparison

Raw average ns/invocation divided by 1,000 operations. Lower is better.

Both array lookup results are excluded from historical gain claims: the old workload does not consume the returned value. Even where CPU-query calls remain, its pointer/value work can simplify after inlining. Use the independent consuming workloads below.

| Operation | Status | Initial ns | Initial/kernel | Previous ns | Current ns | Same-run kernel ns | Current/kernel | vs. initial | vs. previous |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | JIT (separate consuming test) | 15.94 | 4.22x | 1.18 | Not comparable | 3.81 | Separate test | — | — |
| Ordinary array update | Optimized (C++) | 23.51 | 1.78x | 11.62 | 11.53 | 13.21 | 0.87x | -51.0% | -0.8% |
| Ordinary hash lookup | Optimized (C++) | 58.47 | 2.04x | 26.97 | 27.19 | 29.06 | 0.94x | -53.5% | +0.8% |
| Ordinary hash update | Already faster; shared-path gains | 63.18 | 0.67x | 34.54 | 35.07 | 93.20 | 0.38x | -44.5% | +1.5% |
| Ordinary hash delete | Already faster; shared-path gains | 42.64 | 0.40x | 37.06 | 43.69 | 107.88 | 0.40x | +2.5% | +17.9% |
| Per-CPU array lookup | JIT (separate consuming test) | 27.73 | 6.51x | 4.11 | Not comparable | 4.41 | Separate test | — | — |
| Per-CPU array update | Optimized (C++) | 75.93 | 4.55x | 14.55 | 14.39 | 16.60 | 0.87x | -81.0% | -1.1% |
| Per-CPU hash lookup | Optimized (C++) | 141.25 | 4.97x | 27.70 | 27.74 | 28.85 | 0.96x | -80.4% | +0.2% |
| Per-CPU hash update | Optimized (C++) | 220.96 | 3.26x | 44.45 | 44.50 | 67.84 | 0.66x | -79.9% | +0.1% |
| Per-CPU hash delete | Optimized (C++) | 986.68 | 6.50x | 99.89 | 99.81 | 151.89 | 0.66x | -89.9% | -0.1% |

## Consuming per-cpu array lookup (6 paired rounds)

Runtime keys, actual value reads and checked checksum; raw ns/element includes probe/key generation/value reads/result accounting. Not comparable to the historical pure map-operation table.

| Case | Previous INLINE | Current INLINE | Kernel | Current/kernel | vs. previous INLINE |
|---|---:|---:|---:|---:|---:|
| hit | 5.15 | 5.15 | 6.86 | 0.75x | -0.0% |
| mixed | 11.51 | 10.65 | 12.74 | 0.84x | -7.4% |

## Consuming ordinary array lookup (6 paired rounds)

Runtime keys, actual value reads and checked checksum; raw ns/element includes probe/key generation/value reads/result accounting. Not comparable to the historical pure map-operation table.

| Case | Previous INLINE | Current INLINE | Kernel | Current/kernel | vs. previous INLINE |
|---|---:|---:|---:|---:|---:|
| hit | 3.09 | 3.09 | 5.89 | 0.53x | -0.1% |
| mixed | 9.65 | 8.45 | 12.32 | 0.69x | -12.4% |

# Full ten-round regression comparison

Raw average ns/invocation divided by 1,000 operations. Lower is better.

Ordinary array lookup is excluded from historical gain claims: JIT can simplify its unused result. See the consuming-workload result below. Other operations retain their implementation path.

| Operation | Status | Initial ns | Initial/kernel | Previous ns | Current ns | Same-run kernel ns | Current/kernel | vs. initial | vs. previous |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | JIT (separate consuming test) | 15.94 | 4.22x | 6.77 | Not comparable | 3.79 | Separate test | — | — |
| Ordinary array update | Optimized (C++) | 23.51 | 1.78x | 11.54 | 11.55 | 13.08 | 0.88x | -50.9% | +0.0% |
| Ordinary hash lookup | Optimized (C++) | 58.47 | 2.04x | 27.11 | 26.98 | 27.77 | 0.97x | -53.9% | -0.5% |
| Ordinary hash update | Already faster | 63.18 | 0.67x | 33.86 | 34.30 | 95.41 | 0.36x | -45.7% | +1.3% |
| Ordinary hash delete | Already faster | 42.64 | 0.40x | 36.67 | 37.17 | 108.89 | 0.34x | -12.8% | +1.4% |
| Per-CPU array lookup | Optimized (C++) | 27.73 | 6.51x | 10.76 | 10.88 | 4.41 | 2.47x | -60.8% | +1.1% |
| Per-CPU array update | Optimized (C++) | 75.93 | 4.55x | 14.53 | 14.52 | 16.60 | 0.87x | -80.9% | -0.1% |
| Per-CPU hash lookup | Optimized (C++) | 141.25 | 4.97x | 27.71 | 27.72 | 29.75 | 0.93x | -80.4% | +0.0% |
| Per-CPU hash update | Optimized (C++) | 220.96 | 3.26x | 44.38 | 44.44 | 67.82 | 0.66x | -79.9% | +0.1% |
| Per-CPU hash delete | Optimized (C++) | 986.68 | 6.50x | 98.70 | 99.86 | 151.92 | 0.66x | -89.9% | +1.2% |

## Consuming ordinary-array lookup (six paired rounds)

Runtime keys, actual value reads and verified checksum; raw ns/element includes that extra work. Not comparable to the table above.

| Case | Inline off | Inline on | Kernel | Inline/kernel | vs. off |
|---|---:|---:|---:|---:|---:|
| hit | 7.90 | 3.09 | 5.88 | 0.53x | -60.8% |
| mixed | 17.54 | 9.63 | 12.24 | 0.79x | -45.1% |

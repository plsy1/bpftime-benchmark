# 177a1dc baseline: full ten rounds

Raw average ns/invocation divided by 1,000 operations. Lower is better. Current = codex/uprobe-official-177a1dc465ff 5466f0d, LLVM 16. Previous = array-jit-default-delete-fix-20261021 (c0939bd on 8da867d541f8, LLVM 16), whose kernel denominators are a different run.

Both array lookup results are excluded from historical gain claims: the old workload does not consume the returned value. Use the independent consuming workloads below.

| Operation | Status | Initial ns | Initial/kernel | Previous ns | Previous kernel ns | Current ns | Same-run kernel ns | Current/kernel | vs. initial | vs. previous |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | JIT (separate consuming test) | 15.94 | 4.22x | 1.18 | 3.74 | 1.17 (not comparable) | 3.81 | Separate test | — | — |
| Ordinary array update | Optimized (C++) | 23.51 | 1.78x | 11.55 | 13.04 | 11.57 | 13.09 | 0.88x | -50.8% | +0.1% |
| Ordinary hash lookup | Optimized (C++) | 58.47 | 2.04x | 26.90 | 29.00 | 27.10 | 28.61 | 0.95x | -53.7% | +0.7% |
| Ordinary hash update | Already faster; shared-path gains | 63.18 | 0.67x | 34.21 | 94.95 | 34.11 | 93.18 | 0.37x | -46.0% | -0.3% |
| Ordinary hash delete | Already faster; shared-path gains | 42.64 | 0.40x | 36.64 | 108.06 | 36.32 | 107.95 | 0.34x | -14.8% | -0.9% |
| Per-CPU array lookup | JIT (separate consuming test) | 27.73 | 6.51x | 4.12 | 4.41 | 4.12 (not comparable) | 4.41 | Separate test | — | — |
| Per-CPU array update | Optimized (C++) | 75.93 | 4.55x | 14.41 | 16.58 | 14.37 | 16.63 | 0.86x | -81.1% | -0.3% |
| Per-CPU hash lookup | Optimized (C++) | 141.25 | 4.97x | 27.74 | 29.37 | 27.73 | 29.39 | 0.94x | -80.4% | -0.0% |
| Per-CPU hash update | Optimized (C++) | 220.96 | 3.26x | 44.42 | 68.11 | 44.62 | 68.22 | 0.65x | -79.8% | +0.4% |
| Per-CPU hash delete | Optimized (C++) | 986.68 | 6.50x | 98.77 | 149.54 | 98.72 | 151.91 | 0.65x | -90.0% | -0.0% |

## Consuming ordinary array lookup (6 paired rounds)

Runtime keys, actual value reads and checked checksum; raw ns/element includes probe/key generation/value reads/result accounting. Not comparable to the historical pure map-operation table. Previous = frozen c0939bd artifacts measured in this same run.

| Case | Previous c0939bd (8da) | Current 5466f0d (177a1dc) | Kernel | Current/kernel | vs. previous | Current wins |
|---|---:|---:|---:|---:|---:|---:|
| hit | 3.09 | 3.10 | 5.89 | 0.53x | +0.1% | 1/6 |
| mixed | 8.44 | 8.46 | 12.35 | 0.69x | +0.2% | 0/6 |

## Consuming per-cpu array lookup (6 paired rounds)

Runtime keys, actual value reads and checked checksum; raw ns/element includes probe/key generation/value reads/result accounting. Not comparable to the historical pure map-operation table. Previous = frozen c0939bd artifacts measured in this same run.

| Case | Previous c0939bd (8da) | Current 5466f0d (177a1dc) | Kernel | Current/kernel | vs. previous | Current wins |
|---|---:|---:|---:|---:|---:|---:|
| hit | 5.18 | 5.18 | 6.84 | 0.76x | -0.0% | 4/6 |
| mixed | 10.65 | 10.65 | 12.80 | 0.83x | +0.1% | 2/6 |

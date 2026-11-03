# x86 NUC full ten rounds, dev d5279469 (full10-4.4ghz-turbo)

Lower is better. Each leg ran kernel, userspace and embedded groups in one benchmark.py --iter 10 session; ratios use the kernel group of the same leg. 42595149 is the frozen build of the 11/03 sanity check, measured again in this session. Array lookup rows do not consume the returned value. The suite's hash update rewrites existing keys. Sample 6 of the 42595149 leg is an outlier in the per-CPU rows (per-CPU array lookup 7.65 against 1.28 ns), hence the median column.

## Map operations (ns per operation)

| Operation | d5279469 ns/op | kernel (its round) | d5279469/kernel | 42595149 ns/op | kernel (its round) | 42595149/kernel | d5279469 vs 42595149 | same, medians | 6ab9cbfe 11/02 | Jetson 518a6b99/kernel |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 0.45 | 1.82 | 0.25x | 0.45 | 1.81 | 0.25x | +0.1% | +0.1% | 0.45 | 0.31x |
| Ordinary array update | 3.39 | 7.44 | 0.46x | 3.42 | 7.43 | 0.46x | -0.9% | -2.1% | 3.36 | 0.88x |
| Ordinary hash lookup | 4.77 | 10.14 | 0.47x | 5.30 | 10.61 | 0.50x | -10.1% | -10.9% | 5.28 | 0.63x |
| Ordinary hash update | 12.01 | 34.65 | 0.35x | 10.04 | 35.00 | 0.29x | +19.6% | +19.7% | 11.98 | 0.36x |
| Ordinary hash delete | 14.61 | 34.25 | 0.43x | 15.57 | 34.18 | 0.46x | -6.2% | -6.6% | 13.71 | 0.33x |
| Per-CPU array lookup | 1.28 | 1.82 | 0.70x | 1.92 | 1.82 | 1.05x | -33.2% | -0.0% | 1.28 | 0.91x |
| Per-CPU array update | 4.85 | 7.54 | 0.64x | 5.69 | 7.55 | 0.75x | -14.7% | -4.4% | 4.73 | 0.90x |
| Per-CPU hash lookup | 7.02 | 12.64 | 0.55x | 7.61 | 10.69 | 0.71x | -7.8% | -2.7% | 10.24 | 0.84x |
| Per-CPU hash update | 11.99 | 25.90 | 0.46x | 12.24 | 25.61 | 0.48x | -2.0% | +2.4% | 16.14 | 0.51x |
| Per-CPU hash delete | 25.65 | 111.63 | 0.23x | 25.68 | 112.22 | 0.23x | -0.1% | +1.5% | 31.87 | 0.43x |

## Probe and helper overhead (ns per call)

| Operation | d5279469 ns | kernel (its round) | d5279469/kernel | 42595149 ns | kernel (its round) | 42595149/kernel | d5279469 vs 42595149 | same, medians | 6ab9cbfe 11/02 | Jetson 518a6b99/kernel |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| uprobe | 122.92 | 1110.27 | 0.11x | 124.88 | 1105.99 | 0.11x | -1.6% | -1.3% | 122.81 | 0.16x |
| uretprobe | 184.43 | 1297.82 | 0.14x | 186.31 | 1294.39 | 0.14x | -1.0% | -0.4% | 183.80 | 0.16x |
| uprobe + uretprobe | 216.16 | 1355.60 | 0.16x | 216.96 | 1339.73 | 0.16x | -0.4% | -0.2% | 212.96 | 0.18x |
| probe read | 1506.66 | 19097.20 | 0.08x | 1507.93 | 19171.60 | 0.08x | -0.1% | -0.1% | 1512.48 | 0.28x |
| probe write | 1279.90 | 19134.64 | 0.07x | 1282.45 | 19140.86 | 0.07x | -0.2% | -0.2% | 1280.62 | 0.25x |

## Run-to-run spread (coefficient of variation of the 10 samples, %)

| Operation | d5279469 | 42595149 | kernel (d5279469 round) | kernel (42595149 round) |
|---|---:|---:|---:|---:|
| Ordinary array lookup | 0.1 | 0.2 | 0.2 | 0.3 |
| Ordinary array update | 2.1 | 0.1 | 0.8 | 0.8 |
| Ordinary hash lookup | 4.3 | 1.3 | 0.4 | 0.2 |
| Ordinary hash update | 0.9 | 0.8 | 0.2 | 0.2 |
| Ordinary hash delete | 1.7 | 0.3 | 0.2 | 0.2 |
| Per-CPU array lookup | 0.1 | 99.7 | 0.2 | 0.3 |
| Per-CPU array update | 1.6 | 34.1 | 1.0 | 0.9 |
| Per-CPU hash lookup | 0.8 | 16.2 | 0.9 | 0.5 |
| Per-CPU hash update | 0.4 | 13.3 | 0.5 | 0.4 |
| Per-CPU hash delete | 0.6 | 4.6 | 0.2 | 0.2 |
| uprobe | 0.6 | 0.7 | 0.2 | 0.4 |
| uretprobe | 0.4 | 1.4 | 0.2 | 0.3 |
| uprobe + uretprobe | 0.4 | 0.4 | 2.3 | 0.4 |
| probe read | 0.1 | 0.2 | 3.1 | 3.1 |
| probe write | 0.2 | 0.2 | 0.4 | 0.6 |

Embedded (dev-d5279469): embed avg 63.66

Embedded (dev-42595149): embed avg 62.94

Embedded (6ab9cbfe, 11/02 ten rounds): embed avg 63.82

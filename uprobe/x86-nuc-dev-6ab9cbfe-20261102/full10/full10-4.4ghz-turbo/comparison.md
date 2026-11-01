# x86 NUC full ten rounds, dev 6ab9cbfe (full10-4.4ghz-turbo)

Lower is better. Each leg ran kernel, userspace and embedded groups in one benchmark.py --iter 10 session; ratios use the kernel leg of the same session. a7d54d7e is the frozen 10/26 dev build, measured again in this session. Array lookup rows do not consume the returned value. Jetson column: benchmark-results/jetson uprobe/dev-6ab9cbfe-20261101 (CPU 5 at 1.728 GHz, LLVM 16).

## Map operations (ns per operation)

| Operation | 6ab9cbfe ns/op | kernel (its round) | 6ab9cbfe/kernel | a7d54d7e ns/op | kernel (its round) | a7d54d7e/kernel | 6ab9cbfe vs a7d54d7e | a7d54d7e 10/26 | official/kernel 10/26 | Jetson 6ab9cbfe/kernel |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 0.45 | 1.82 | 0.24x | 0.45 | 1.82 | 0.24x | -0.3% | 0.45 | 3.19x | 0.31x |
| Ordinary array update | 3.36 | 7.42 | 0.45x | 3.37 | 7.43 | 0.45x | -0.2% | 3.58 | 1.20x | 0.89x |
| Ordinary hash lookup | 5.28 | 10.07 | 0.52x | 8.61 | 10.35 | 0.83x | -38.7% | 8.60 | 2.08x | 0.96x |
| Ordinary hash update | 11.98 | 35.22 | 0.34x | 10.05 | 34.59 | 0.29x | +19.3% | 10.02 | 0.67x | 0.45x |
| Ordinary hash delete | 13.71 | 34.48 | 0.40x | 10.16 | 34.44 | 0.30x | +34.9% | 10.16 | 0.53x | 0.41x |
| Per-CPU array lookup | 1.28 | 1.83 | 0.70x | 1.28 | 1.82 | 0.71x | -0.0% | 1.28 | 5.49x | 0.91x |
| Per-CPU array update | 4.73 | 7.43 | 0.64x | 4.76 | 7.47 | 0.64x | -0.6% | 4.74 | 2.43x | 0.90x |
| Per-CPU hash lookup | 10.24 | 10.76 | 0.95x | 6.98 | 10.94 | 0.64x | +46.7% | 6.99 | 4.25x | 1.15x |
| Per-CPU hash update | 16.14 | 25.64 | 0.63x | 12.65 | 25.78 | 0.49x | +27.5% | 12.63 | 2.79x | 0.85x |
| Per-CPU hash delete | 31.87 | 111.53 | 0.29x | 30.47 | 113.44 | 0.27x | +4.6% | 30.51 | 0.58x | 0.70x |

## Probe and helper overhead (ns per call)

| Operation | 6ab9cbfe ns | kernel (its round) | 6ab9cbfe/kernel | a7d54d7e ns | kernel (its round) | a7d54d7e/kernel | 6ab9cbfe vs a7d54d7e | a7d54d7e 10/26 | official/kernel 10/26 | Jetson 6ab9cbfe/kernel |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| uprobe | 122.81 | 1113.52 | 0.11x | 123.60 | 1105.34 | 0.11x | -0.6% | 123.99 | 0.11x | 0.16x |
| uretprobe | 183.80 | 1296.90 | 0.14x | 182.95 | 1294.05 | 0.14x | +0.5% | 183.79 | 0.14x | 0.16x |
| uprobe + uretprobe | 212.96 | 1339.80 | 0.16x | 214.33 | 1338.71 | 0.16x | -0.6% | 214.38 | 0.16x | 0.18x |
| probe read | 1512.48 | 18924.40 | 0.08x | 1506.19 | 19057.31 | 0.08x | +0.4% | 1507.45 | 0.09x | 0.25x |
| probe write | 1280.62 | 19117.50 | 0.07x | 1282.59 | 19154.55 | 0.07x | -0.2% | 1285.24 | 0.08x | 0.24x |

## Kernel legs: this session against 10/26 (same workload binaries)

| Operation | kernel (6ab9cbfe round) | kernel (a7d54d7e round) | kernel 10/26 (dev round) | rounds differ | vs 10/26 |
|---|---:|---:|---:|---:|---:|
| Ordinary array lookup | 1.82 | 1.82 | 1.81 | +0.3% | +0.5% |
| Ordinary array update | 7.42 | 7.43 | 7.44 | +0.1% | -0.3% |
| Ordinary hash lookup | 10.07 | 10.35 | 10.57 | +2.8% | -4.7% |
| Ordinary hash update | 35.22 | 34.59 | 34.47 | -1.8% | +2.2% |
| Ordinary hash delete | 34.48 | 34.44 | 34.36 | -0.1% | +0.3% |
| Per-CPU array lookup | 1.83 | 1.82 | 1.84 | -0.5% | -0.7% |
| Per-CPU array update | 7.43 | 7.47 | 7.43 | +0.5% | +0.0% |
| Per-CPU hash lookup | 10.76 | 10.94 | 10.61 | +1.7% | +1.4% |
| Per-CPU hash update | 25.64 | 25.78 | 25.81 | +0.5% | -0.7% |
| Per-CPU hash delete | 111.53 | 113.44 | 112.21 | +1.7% | -0.6% |
| uprobe | 1113.52 | 1105.34 | 1107.91 | -0.7% | +0.5% |
| uretprobe | 1296.90 | 1294.05 | 1292.20 | -0.2% | +0.4% |
| uprobe + uretprobe | 1339.80 | 1338.71 | 1337.72 | -0.1% | +0.2% |
| probe read | 18924.40 | 19057.31 | 18937.77 | +0.7% | -0.1% |
| probe write | 19117.50 | 19154.55 | 19130.42 | +0.2% | -0.1% |

## Run-to-run spread (coefficient of variation of the 10 samples, %)

| Operation | 6ab9cbfe | a7d54d7e | kernel (6ab9cbfe round) | kernel (a7d54d7e round) |
|---|---:|---:|---:|---:|
| Ordinary array lookup | 0.2 | 0.3 | 0.3 | 0.9 |
| Ordinary array update | 1.2 | 1.2 | 0.2 | 0.2 |
| Ordinary hash lookup | 0.2 | 0.1 | 0.8 | 0.7 |
| Ordinary hash update | 1.0 | 0.7 | 0.3 | 0.4 |
| Ordinary hash delete | 0.4 | 0.3 | 0.4 | 0.2 |
| Per-CPU array lookup | 0.2 | 0.2 | 0.2 | 0.2 |
| Per-CPU array update | 0.2 | 1.2 | 0.1 | 0.5 |
| Per-CPU hash lookup | 0.2 | 0.2 | 0.5 | 0.4 |
| Per-CPU hash update | 0.6 | 0.5 | 0.4 | 0.2 |
| Per-CPU hash delete | 0.4 | 0.2 | 0.1 | 0.1 |
| uprobe | 0.6 | 0.5 | 1.4 | 0.4 |
| uretprobe | 0.2 | 0.2 | 0.2 | 0.5 |
| uprobe + uretprobe | 0.8 | 0.5 | 0.3 | 0.6 |
| probe read | 0.9 | 0.1 | 3.4 | 2.9 |
| probe write | 0.2 | 0.1 | 0.2 | 0.1 |

Embedded (dev-6ab9cbfe): embed avg 63.82

Embedded (a7d54d7e): embed avg 63.28

Embedded (a7d54d7e, 10/26): embed avg 63.09

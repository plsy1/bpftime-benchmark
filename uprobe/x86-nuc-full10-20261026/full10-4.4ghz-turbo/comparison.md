# x86 NUC full ten rounds (full10-4.4ghz-turbo)

Lower is better. Each leg ran kernel, userspace and embedded groups in one benchmark.py --iter 10 session; ratios use the kernel leg of the same session. Array lookup rows do not consume the returned value (the JIT inlines the lookup in the dev branch); see the Jetson consuming tests before claiming gains there. Jetson column: benchmark-results/jetson uprobe/btf-kernel-20261026/full10-a7d54d7e-kernel (CPU 5 at 1.728 GHz, LLVM 16).

## Map operations (ns per operation)

| Operation | dev ns/op | kernel (dev round) | dev/kernel | official ns/op | kernel (official round) | official/kernel | dev vs official | kernel rounds differ | Jetson a7d54d7e/kernel |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 0.45 | 1.81 | 0.25x | 5.78 | 1.81 | 3.19x | -92.3% | +0.1% | 0.31x |
| Ordinary array update | 3.58 | 7.44 | 0.48x | 8.94 | 7.45 | 1.20x | -60.0% | +0.1% | 0.88x |
| Ordinary hash lookup | 8.60 | 10.57 | 0.81x | 20.65 | 9.94 | 2.08x | -58.4% | -5.9% | 0.82x |
| Ordinary hash update | 10.02 | 34.47 | 0.29x | 23.48 | 35.09 | 0.67x | -57.3% | +1.8% | 0.29x |
| Ordinary hash delete | 10.16 | 34.36 | 0.30x | 18.33 | 34.35 | 0.53x | -44.6% | -0.0% | 0.26x |
| Per-CPU array lookup | 1.28 | 1.84 | 0.70x | 9.98 | 1.82 | 5.49x | -87.2% | -1.3% | 0.95x |
| Per-CPU array update | 4.74 | 7.43 | 0.64x | 18.15 | 7.48 | 2.43x | -73.9% | +0.7% | 0.87x |
| Per-CPU hash lookup | 6.99 | 10.61 | 0.66x | 45.15 | 10.63 | 4.25x | -84.5% | +0.2% | 0.94x |
| Per-CPU hash update | 12.63 | 25.81 | 0.49x | 70.64 | 25.33 | 2.79x | -82.1% | -1.9% | 0.66x |
| Per-CPU hash delete | 30.51 | 112.21 | 0.27x | 64.50 | 111.95 | 0.58x | -52.7% | -0.2% | 0.67x |

## Probe and helper overhead (ns per call)

| Operation | dev ns | kernel (dev round) | dev/kernel | official ns | kernel (official round) | official/kernel | dev vs official | kernel rounds differ | Jetson a7d54d7e/kernel |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| uprobe | 123.99 | 1107.91 | 0.11x | 122.92 | 1102.96 | 0.11x | +0.9% | -0.4% | 0.16x |
| uretprobe | 183.79 | 1292.20 | 0.14x | 182.40 | 1290.28 | 0.14x | +0.8% | -0.1% | 0.16x |
| uprobe + uretprobe | 214.38 | 1337.72 | 0.16x | 211.95 | 1337.22 | 0.16x | +1.1% | -0.0% | 0.18x |
| probe read | 1507.45 | 18937.77 | 0.08x | 1698.57 | 18956.95 | 0.09x | -11.3% | +0.1% | 0.26x |
| probe write | 1285.24 | 19130.42 | 0.07x | 1506.46 | 19157.87 | 0.08x | -14.7% | +0.1% | 0.24x |

## Run-to-run spread (coefficient of variation of the 10 samples, %)

| Operation | dev | official | kernel (dev round) | kernel (official round) |
|---|---:|---:|---:|---:|
| Ordinary array lookup | 0.2 | 0.1 | 0.2 | 0.3 |
| Ordinary array update | 0.5 | 1.4 | 1.0 | 0.5 |
| Ordinary hash lookup | 0.1 | 2.0 | 0.2 | 0.5 |
| Ordinary hash update | 0.8 | 2.5 | 0.2 | 0.5 |
| Ordinary hash delete | 0.3 | 3.7 | 0.3 | 0.3 |
| Per-CPU array lookup | 0.1 | 1.2 | 3.4 | 0.2 |
| Per-CPU array update | 0.2 | 0.2 | 0.1 | 0.9 |
| Per-CPU hash lookup | 0.2 | 0.2 | 0.6 | 0.2 |
| Per-CPU hash update | 0.5 | 0.2 | 0.5 | 0.1 |
| Per-CPU hash delete | 0.3 | 0.1 | 0.1 | 0.1 |
| uprobe | 0.8 | 0.4 | 0.4 | 0.3 |
| uretprobe | 0.7 | 0.4 | 0.3 | 0.4 |
| uprobe + uretprobe | 0.4 | 0.6 | 0.2 | 0.4 |
| probe read | 0.2 | 1.3 | 3.4 | 3.2 |
| probe write | 0.4 | 0.1 | 0.4 | 1.1 |

Embedded (dev): embed avg 63.09

Embedded (official): embed avg 63.61

# c3090b59 full ten rounds (userspace + embedded, no kernel arm)

Mean of ten raw ns/invocation samples divided by 1,000 operations (probe rows: ns/invocation). Baseline = 5466f0d (`baseline-177a1dc-jit-llvm16-20261021`); globals = bdd9835 (`array-jit-global-data-20261021/formal`); both are different runs. The kernel arm was not run: "/kernel" uses the baseline run's kernel (not same-run). Array lookup rows do not consume the result and are not gain metrics.

| Operation | Baseline ns | bdd9835 ns | Current ns | Current median | Samples min-max | vs. baseline | vs. bdd9835 | /kernel (baseline run) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 1.17 | 1.17 | 1.17 | 1.17 | 1.17-1.17 | -0.2% | -0.1% | 0.31x |
| Ordinary array update | 11.57 | 11.61 | 11.55 | 11.54 | 11.51-11.60 | -0.2% | -0.5% | 0.88x |
| Ordinary hash lookup | 27.10 | 27.22 | 26.98 | 27.06 | 26.80-27.09 | -0.4% | -0.9% | 0.94x |
| Ordinary hash update | 34.11 | 35.41 | 34.92 | 35.86 | 33.00-36.34 | +2.4% | -1.4% | 0.37x |
| Ordinary hash delete | 36.32 | 36.28 | 36.24 | 36.25 | 36.03-36.39 | -0.2% | -0.1% | 0.34x |
| Per-CPU array lookup | 4.12 | 4.11 | 4.10 | 4.11 | 4.10-4.12 | -0.4% | -0.1% | 0.93x |
| Per-CPU array update | 14.37 | 14.38 | 14.53 | 14.52 | 14.48-14.61 | +1.2% | +1.1% | 0.87x |
| Per-CPU hash lookup | 27.73 | 27.76 | 27.73 | 27.72 | 27.57-27.85 | -0.0% | -0.1% | 0.94x |
| Per-CPU hash update | 44.62 | 44.47 | 44.38 | 44.40 | 44.29-44.45 | -0.5% | -0.2% | 0.65x |
| Per-CPU hash delete | 98.72 | 98.82 | 98.84 | 98.87 | 98.30-99.32 | +0.1% | +0.0% | 0.65x |
| uprobe | 198.89 | 198.84 | 198.71 | 198.66 | 196.93-200.16 | -0.1% | -0.1% | 0.16x |
| uretprobe | 270.29 | 268.60 | 271.11 | 270.21 | 269.53-275.84 | +0.3% | +0.9% | 0.17x |
| uprobe + uretprobe | 323.76 | 323.15 | 324.54 | 324.25 | 323.17-326.67 | +0.2% | +0.4% | 0.18x |
| probe read | 4584.51 | 4569.14 | 4566.56 | 4566.24 | 4565.43-4568.95 | -0.4% | -0.1% | 0.25x |
| probe write | 4273.78 | 4273.09 | 4525.46 | 4383.84 | 4272.94-4848.24 | +5.9% | +5.9% | 0.26x |

Embedded `embed`: current 157.87, baseline 159.49 (-1.0%).

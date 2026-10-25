# a7d54d7e full ten rounds (userspace + embedded, no kernel arm)

Mean of ten raw ns/invocation samples divided by 1,000 operations (probe rows: ns/invocation). Baseline = 5466f0d (`baseline-177a1dc-jit-llvm16-20261021`); previous = f9d2d9fc (`realprog-next-20261025/full10-f9d2d9fc`); both are different runs. The kernel arm was not run: "/kernel" uses the baseline run's kernel (not same-run). Array lookup rows do not consume the result and are not gain metrics.

| Operation | Baseline ns | f9d2d9fc ns | Current ns | Current median | Samples min-max | vs. baseline | vs. f9d2d9fc | /kernel (baseline run) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 1.17 | 1.17 | 1.18 | 1.18 | 1.17-1.18 | +0.3% | +0.2% | 0.31x |
| Ordinary array update | 11.57 | 11.55 | 11.54 | 11.51 | 11.48-11.74 | -0.2% | -0.0% | 0.88x |
| Ordinary hash lookup | 27.10 | 23.12 | 23.13 | 23.01 | 22.86-23.75 | -14.6% | +0.1% | 0.81x |
| Ordinary hash update | 34.11 | 27.70 | 28.13 | 27.19 | 26.51-34.77 | -17.5% | +1.5% | 0.30x |
| Ordinary hash delete | 36.32 | 27.93 | 27.91 | 27.56 | 27.32-28.63 | -23.1% | -0.0% | 0.26x |
| Per-CPU array lookup | 4.12 | 4.11 | 4.12 | 4.12 | 4.11-4.13 | -0.1% | +0.1% | 0.93x |
| Per-CPU array update | 14.37 | 14.46 | 14.51 | 14.53 | 14.38-14.66 | +1.0% | +0.3% | 0.87x |
| Per-CPU hash lookup | 27.73 | 27.72 | 27.72 | 27.73 | 27.60-27.83 | -0.0% | +0.0% | 0.94x |
| Per-CPU hash update | 44.62 | 44.40 | 44.43 | 44.43 | 44.33-44.52 | -0.4% | +0.0% | 0.65x |
| Per-CPU hash delete | 98.72 | 98.94 | 99.60 | 99.62 | 99.37-99.67 | +0.9% | +0.7% | 0.66x |
| uprobe | 198.89 | 196.87 | 200.85 | 200.93 | 198.52-203.59 | +1.0% | +2.0% | 0.16x |
| uretprobe | 270.29 | 268.17 | 273.33 | 273.39 | 272.11-275.93 | +1.1% | +1.9% | 0.17x |
| uprobe + uretprobe | 323.76 | 324.97 | 327.79 | 327.18 | 326.09-331.23 | +1.2% | +0.9% | 0.19x |
| probe read | 4584.51 | 4578.77 | 4569.09 | 4568.59 | 4566.62-4573.22 | -0.3% | -0.2% | 0.25x |
| probe write | 4273.78 | 4272.27 | 4288.08 | 4278.13 | 4273.00-4362.48 | +0.3% | +0.4% | 0.24x |

Embedded `embed`: current 156.91, baseline 159.49 (-1.6%).

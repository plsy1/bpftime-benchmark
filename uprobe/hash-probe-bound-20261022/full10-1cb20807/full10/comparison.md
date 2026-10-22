# 1cb20807 full ten rounds (userspace + embedded, no kernel arm)

Mean of ten raw ns/invocation samples divided by 1,000 operations (probe rows: ns/invocation). Baseline = 5466f0d (`baseline-177a1dc-jit-llvm16-20261021`); previous = c3090b59 (`hash-helper-fixes-20261022/full10-c3090b59`); both are different runs. The kernel arm was not run: "/kernel" uses the baseline run's kernel (not same-run). Array lookup rows do not consume the result and are not gain metrics.

| Operation | Baseline ns | c3090b59 ns | Current ns | Current median | Samples min-max | vs. baseline | vs. c3090b59 | /kernel (baseline run) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 1.17 | 1.17 | 1.17 | 1.17 | 1.17-1.19 | +0.1% | +0.3% | 0.31x |
| Ordinary array update | 11.57 | 11.55 | 11.54 | 11.53 | 11.48-11.65 | -0.2% | -0.0% | 0.88x |
| Ordinary hash lookup | 27.10 | 26.98 | 24.27 | 24.09 | 23.70-25.04 | -10.4% | -10.0% | 0.85x |
| Ordinary hash update | 34.11 | 34.92 | 29.74 | 28.38 | 27.31-37.56 | -12.8% | -14.8% | 0.32x |
| Ordinary hash delete | 36.32 | 36.24 | 36.86 | 36.74 | 36.64-37.17 | +1.5% | +1.7% | 0.34x |
| Per-CPU array lookup | 4.12 | 4.10 | 4.11 | 4.11 | 4.10-4.12 | -0.2% | +0.2% | 0.93x |
| Per-CPU array update | 14.37 | 14.53 | 14.57 | 14.52 | 14.47-15.13 | +1.4% | +0.2% | 0.88x |
| Per-CPU hash lookup | 27.73 | 27.73 | 27.86 | 27.76 | 27.62-28.93 | +0.5% | +0.5% | 0.95x |
| Per-CPU hash update | 44.62 | 44.38 | 44.62 | 44.51 | 44.43-45.64 | -0.0% | +0.5% | 0.65x |
| Per-CPU hash delete | 98.72 | 98.84 | 99.05 | 98.75 | 98.66-100.28 | +0.3% | +0.2% | 0.65x |
| uprobe | 198.89 | 198.71 | 198.36 | 197.76 | 197.46-201.18 | -0.3% | -0.2% | 0.16x |
| uretprobe | 270.29 | 271.11 | 269.41 | 269.31 | 268.13-270.68 | -0.3% | -0.6% | 0.16x |
| uprobe + uretprobe | 323.76 | 324.54 | 322.83 | 322.62 | 321.93-324.12 | -0.3% | -0.5% | 0.18x |
| probe read | 4584.51 | 4566.56 | 4564.92 | 4564.37 | 4563.23-4568.53 | -0.4% | -0.0% | 0.25x |
| probe write | 4273.78 | 4525.46 | 4272.35 | 4271.78 | 4271.14-4275.30 | -0.0% | -5.6% | 0.24x |

Embedded `embed`: current 158.32, baseline 159.49 (-0.7%).

# af10949b full ten rounds (userspace + embedded, no kernel arm)

Mean of ten raw ns/invocation samples divided by 1,000 operations (probe rows: ns/invocation). Baseline = 5466f0d (`baseline-177a1dc-jit-llvm16-20261021`); previous = f26cc335 (`direct-map-helpers-20261022/full10-f26cc335`); both are different runs. The kernel arm was not run: "/kernel" uses the baseline run's kernel (not same-run). Array lookup rows do not consume the result and are not gain metrics.

| Operation | Baseline ns | f26cc335 ns | Current ns | Current median | Samples min-max | vs. baseline | vs. f26cc335 | /kernel (baseline run) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 1.17 | 1.17 | 1.17 | 1.17 | 1.17-1.18 | +0.1% | +0.1% | 0.31x |
| Ordinary array update | 11.57 | 11.54 | 11.56 | 11.56 | 11.54-11.60 | -0.1% | +0.2% | 0.88x |
| Ordinary hash lookup | 27.10 | 24.28 | 24.11 | 23.80 | 23.75-24.87 | -11.0% | -0.7% | 0.84x |
| Ordinary hash update | 34.11 | 29.44 | 28.29 | 27.23 | 27.14-30.83 | -17.1% | -3.9% | 0.30x |
| Ordinary hash delete | 36.32 | 36.86 | 39.23 | 38.49 | 38.35-41.85 | +8.0% | +6.4% | 0.36x |
| Per-CPU array lookup | 4.12 | 4.12 | 4.10 | 4.10 | 4.10-4.12 | -0.4% | -0.3% | 0.93x |
| Per-CPU array update | 14.37 | 14.51 | 14.26 | 14.27 | 14.18-14.34 | -0.7% | -1.7% | 0.86x |
| Per-CPU hash lookup | 27.73 | 27.74 | 27.67 | 27.66 | 27.62-27.76 | -0.2% | -0.2% | 0.94x |
| Per-CPU hash update | 44.62 | 44.46 | 44.48 | 44.46 | 44.42-44.57 | -0.3% | +0.0% | 0.65x |
| Per-CPU hash delete | 98.72 | 98.86 | 98.76 | 98.53 | 98.44-99.37 | +0.0% | -0.1% | 0.65x |
| uprobe | 198.89 | 199.59 | 198.29 | 198.29 | 196.81-199.90 | -0.3% | -0.6% | 0.16x |
| uretprobe | 270.29 | 270.03 | 268.65 | 268.62 | 267.50-269.72 | -0.6% | -0.5% | 0.16x |
| uprobe + uretprobe | 323.76 | 358.08 | 335.01 | 333.58 | 331.56-340.89 | +3.5% | -6.4% | 0.19x |
| probe read | 4584.51 | 4572.09 | 4567.64 | 4567.82 | 4564.31-4570.01 | -0.4% | -0.1% | 0.25x |
| probe write | 4273.78 | 4547.04 | 4514.26 | 4450.54 | 4272.10-4836.14 | +5.6% | -0.7% | 0.26x |

Embedded `embed`: current 156.73, baseline 159.49 (-1.7%).

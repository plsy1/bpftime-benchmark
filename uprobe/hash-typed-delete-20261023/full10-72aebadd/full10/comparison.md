# 72aebadd full ten rounds (userspace + embedded, no kernel arm)

Mean of ten raw ns/invocation samples divided by 1,000 operations (probe rows: ns/invocation). Baseline = 5466f0d (`baseline-177a1dc-jit-llvm16-20261021`); previous = f26cc335 (`direct-map-helpers-20261022/full10-f26cc335`); both are different runs. The kernel arm was not run: "/kernel" uses the baseline run's kernel (not same-run). Array lookup rows do not consume the result and are not gain metrics.

| Operation | Baseline ns | f26cc335 ns | Current ns | Current median | Samples min-max | vs. baseline | vs. f26cc335 | /kernel (baseline run) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 1.17 | 1.17 | 1.18 | 1.18 | 1.17-1.18 | +0.4% | +0.4% | 0.31x |
| Ordinary array update | 11.57 | 11.54 | 11.54 | 11.52 | 11.50-11.61 | -0.2% | +0.0% | 0.88x |
| Ordinary hash lookup | 27.10 | 24.28 | 24.36 | 24.72 | 23.60-25.22 | -10.1% | +0.3% | 0.85x |
| Ordinary hash update | 34.11 | 29.44 | 29.56 | 30.61 | 27.30-31.37 | -13.3% | +0.4% | 0.32x |
| Ordinary hash delete | 36.32 | 36.86 | 28.22 | 28.54 | 27.22-28.76 | -22.3% | -23.4% | 0.26x |
| Per-CPU array lookup | 4.12 | 4.12 | 4.12 | 4.12 | 4.11-4.13 | -0.1% | +0.0% | 0.93x |
| Per-CPU array update | 14.37 | 14.51 | 14.47 | 14.49 | 14.38-14.51 | +0.8% | -0.2% | 0.87x |
| Per-CPU hash lookup | 27.73 | 27.74 | 27.69 | 27.67 | 27.57-27.84 | -0.2% | -0.2% | 0.94x |
| Per-CPU hash update | 44.62 | 44.46 | 44.77 | 44.79 | 44.68-44.87 | +0.3% | +0.7% | 0.66x |
| Per-CPU hash delete | 98.72 | 98.86 | 99.42 | 99.40 | 99.32-99.53 | +0.7% | +0.6% | 0.65x |
| uprobe | 198.89 | 199.59 | 205.56 | 205.50 | 204.09-207.15 | +3.4% | +3.0% | 0.17x |
| uretprobe | 270.29 | 270.03 | 269.73 | 269.55 | 269.05-271.88 | -0.2% | -0.1% | 0.16x |
| uprobe + uretprobe | 323.76 | 358.08 | 322.47 | 322.57 | 321.61-323.37 | -0.4% | -9.9% | 0.18x |
| probe read | 4584.51 | 4572.09 | 4589.10 | 4590.50 | 4578.29-4596.89 | +0.1% | +0.4% | 0.25x |
| probe write | 4273.78 | 4547.04 | 4281.15 | 4281.05 | 4280.34-4282.70 | +0.2% | -5.8% | 0.24x |

Embedded `embed`: current 156.59, baseline 159.49 (-1.8%).

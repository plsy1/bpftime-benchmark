# f26cc335 full ten rounds (userspace + embedded, no kernel arm)

Mean of ten raw ns/invocation samples divided by 1,000 operations (probe rows: ns/invocation). Baseline = 5466f0d (`baseline-177a1dc-jit-llvm16-20261021`); previous = 1cb20807 (`hash-probe-bound-20261022/full10-1cb20807`); both are different runs. The kernel arm was not run: "/kernel" uses the baseline run's kernel (not same-run). Array lookup rows do not consume the result and are not gain metrics.

| Operation | Baseline ns | 1cb20807 ns | Current ns | Current median | Samples min-max | vs. baseline | vs. 1cb20807 | /kernel (baseline run) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 1.17 | 1.17 | 1.17 | 1.17 | 1.17-1.18 | +0.0% | -0.0% | 0.31x |
| Ordinary array update | 11.57 | 11.54 | 11.54 | 11.54 | 11.50-11.58 | -0.3% | -0.0% | 0.88x |
| Ordinary hash lookup | 27.10 | 24.27 | 24.28 | 24.25 | 23.73-24.93 | -10.4% | +0.0% | 0.85x |
| Ordinary hash update | 34.11 | 29.74 | 29.44 | 29.35 | 27.35-31.52 | -13.7% | -1.0% | 0.32x |
| Ordinary hash delete | 36.32 | 36.86 | 36.86 | 36.82 | 36.60-37.27 | +1.5% | -0.0% | 0.34x |
| Per-CPU array lookup | 4.12 | 4.11 | 4.12 | 4.12 | 4.10-4.13 | -0.1% | +0.1% | 0.93x |
| Per-CPU array update | 14.37 | 14.57 | 14.51 | 14.51 | 14.42-14.64 | +1.0% | -0.4% | 0.87x |
| Per-CPU hash lookup | 27.73 | 27.86 | 27.74 | 27.73 | 27.63-27.86 | +0.0% | -0.4% | 0.94x |
| Per-CPU hash update | 44.62 | 44.62 | 44.46 | 44.45 | 44.37-44.68 | -0.3% | -0.3% | 0.65x |
| Per-CPU hash delete | 98.72 | 99.05 | 98.86 | 98.80 | 98.49-99.43 | +0.1% | -0.2% | 0.65x |
| uprobe | 198.89 | 198.36 | 199.59 | 199.26 | 197.50-202.60 | +0.4% | +0.6% | 0.16x |
| uretprobe | 270.29 | 269.41 | 270.03 | 269.85 | 268.35-273.29 | -0.1% | +0.2% | 0.16x |
| uprobe + uretprobe | 323.76 | 322.83 | 358.08 | 357.46 | 355.44-363.56 | +10.6% | +10.9% | 0.20x |
| probe read | 4584.51 | 4564.92 | 4572.09 | 4572.03 | 4569.75-4574.33 | -0.3% | +0.2% | 0.25x |
| probe write | 4273.78 | 4272.35 | 4547.04 | 4494.58 | 4273.74-4851.94 | +6.4% | +6.4% | 0.26x |

Embedded `embed`: current 157.17, baseline 159.49 (-1.5%).

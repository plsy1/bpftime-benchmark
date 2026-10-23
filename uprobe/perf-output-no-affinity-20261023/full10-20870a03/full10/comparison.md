# 20870a03 full ten rounds (userspace + embedded, no kernel arm)

Mean of ten raw ns/invocation samples divided by 1,000 operations (probe rows: ns/invocation). Baseline = 5466f0d (`baseline-177a1dc-jit-llvm16-20261021`); previous = 72aebadd (`hash-typed-delete-20261023/full10-72aebadd`); both are different runs. The kernel arm was not run: "/kernel" uses the baseline run's kernel (not same-run). Array lookup rows do not consume the result and are not gain metrics.

| Operation | Baseline ns | 72aebadd ns | Current ns | Current median | Samples min-max | vs. baseline | vs. 72aebadd | /kernel (baseline run) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 1.17 | 1.18 | 1.17 | 1.17 | 1.17-1.18 | -0.1% | -0.5% | 0.31x |
| Ordinary array update | 11.57 | 11.54 | 11.53 | 11.53 | 11.49-11.57 | -0.3% | -0.1% | 0.88x |
| Ordinary hash lookup | 27.10 | 24.36 | 24.22 | 24.19 | 23.58-25.00 | -10.6% | -0.6% | 0.85x |
| Ordinary hash update | 34.11 | 29.56 | 29.29 | 29.48 | 27.23-31.29 | -14.1% | -0.9% | 0.31x |
| Ordinary hash delete | 36.32 | 28.22 | 27.94 | 28.11 | 27.18-28.77 | -23.1% | -1.0% | 0.26x |
| Per-CPU array lookup | 4.12 | 4.12 | 4.12 | 4.12 | 4.10-4.13 | -0.1% | -0.0% | 0.93x |
| Per-CPU array update | 14.37 | 14.47 | 14.49 | 14.48 | 14.42-14.65 | +0.9% | +0.1% | 0.87x |
| Per-CPU hash lookup | 27.73 | 27.69 | 27.77 | 27.81 | 27.65-27.89 | +0.1% | +0.3% | 0.95x |
| Per-CPU hash update | 44.62 | 44.77 | 44.55 | 44.55 | 44.45-44.80 | -0.2% | -0.5% | 0.65x |
| Per-CPU hash delete | 98.72 | 99.42 | 99.28 | 99.26 | 98.99-99.82 | +0.6% | -0.1% | 0.65x |
| uprobe | 198.89 | 205.56 | 197.39 | 197.51 | 195.56-198.95 | -0.8% | -4.0% | 0.16x |
| uretprobe | 270.29 | 269.73 | 268.86 | 268.68 | 267.16-271.66 | -0.5% | -0.3% | 0.16x |
| uprobe + uretprobe | 323.76 | 322.47 | 323.77 | 323.25 | 322.23-328.04 | +0.0% | +0.4% | 0.18x |
| probe read | 4584.51 | 4589.10 | 4567.50 | 4567.66 | 4564.97-4569.84 | -0.4% | -0.5% | 0.25x |
| probe write | 4273.78 | 4281.15 | 4273.18 | 4273.25 | 4270.98-4275.54 | -0.0% | -0.2% | 0.24x |

Embedded `embed`: current 157.25, baseline 159.49 (-1.4%).

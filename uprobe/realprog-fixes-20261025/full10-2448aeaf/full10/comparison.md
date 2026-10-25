# 2448aeaf full ten rounds (userspace + embedded, no kernel arm)

Mean of ten raw ns/invocation samples divided by 1,000 operations (probe rows: ns/invocation). Baseline = 5466f0d (`baseline-177a1dc-jit-llvm16-20261021`); previous = bca2ebce (`perf-output-identity-20261024/full10-bca2ebce`); both are different runs. The kernel arm was not run: "/kernel" uses the baseline run's kernel (not same-run). Array lookup rows do not consume the result and are not gain metrics.

| Operation | Baseline ns | bca2ebce ns | Current ns | Current median | Samples min-max | vs. baseline | vs. bca2ebce | /kernel (baseline run) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 1.17 | 1.17 | 1.20 | 1.20 | 1.19-1.20 | +1.9% | +2.1% | 0.31x |
| Ordinary array update | 11.57 | 11.54 | 11.54 | 11.52 | 11.49-11.71 | -0.2% | +0.0% | 0.88x |
| Ordinary hash lookup | 27.10 | 24.62 | 23.20 | 23.18 | 22.87-23.68 | -14.4% | -5.8% | 0.81x |
| Ordinary hash update | 34.11 | 30.58 | 27.65 | 27.86 | 26.61-28.51 | -19.0% | -9.6% | 0.30x |
| Ordinary hash delete | 36.32 | 27.66 | 27.88 | 27.97 | 27.15-28.50 | -23.2% | +0.8% | 0.26x |
| Per-CPU array lookup | 4.12 | 4.11 | 4.12 | 4.12 | 4.10-4.14 | -0.1% | +0.1% | 0.93x |
| Per-CPU array update | 14.37 | 14.30 | 14.43 | 14.41 | 14.34-14.70 | +0.5% | +0.9% | 0.87x |
| Per-CPU hash lookup | 27.73 | 27.70 | 27.71 | 27.71 | 27.61-27.82 | -0.1% | +0.0% | 0.94x |
| Per-CPU hash update | 44.62 | 44.41 | 44.41 | 44.40 | 44.30-44.49 | -0.5% | +0.0% | 0.65x |
| Per-CPU hash delete | 98.72 | 98.72 | 98.87 | 98.88 | 98.79-98.94 | +0.2% | +0.1% | 0.65x |
| uprobe | 198.89 | 199.12 | 199.23 | 199.27 | 197.44-200.65 | +0.2% | +0.1% | 0.16x |
| uretprobe | 270.29 | 271.31 | 267.66 | 267.31 | 265.37-271.97 | -1.0% | -1.3% | 0.16x |
| uprobe + uretprobe | 323.76 | 324.43 | 324.39 | 324.17 | 322.57-328.35 | +0.2% | -0.0% | 0.18x |
| probe read | 4584.51 | 4593.84 | 4580.43 | 4579.77 | 4577.71-4584.38 | -0.1% | -0.3% | 0.25x |
| probe write | 4273.78 | 4273.63 | 4274.65 | 4274.71 | 4273.16-4276.07 | +0.0% | +0.0% | 0.24x |

Embedded `embed`: current 160.44, baseline 159.49 (+0.6%).

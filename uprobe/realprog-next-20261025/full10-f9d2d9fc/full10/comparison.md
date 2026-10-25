# f9d2d9fc full ten rounds (userspace + embedded, no kernel arm)

Mean of ten raw ns/invocation samples divided by 1,000 operations (probe rows: ns/invocation). Baseline = 5466f0d (`baseline-177a1dc-jit-llvm16-20261021`); previous = 2448aeaf (`realprog-fixes-20261025/full10-2448aeaf`); both are different runs. The kernel arm was not run: "/kernel" uses the baseline run's kernel (not same-run). Array lookup rows do not consume the result and are not gain metrics.

| Operation | Baseline ns | 2448aeaf ns | Current ns | Current median | Samples min-max | vs. baseline | vs. 2448aeaf | /kernel (baseline run) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 1.17 | 1.20 | 1.17 | 1.17 | 1.17-1.18 | +0.0% | -1.9% | 0.31x |
| Ordinary array update | 11.57 | 11.54 | 11.55 | 11.54 | 11.51-11.60 | -0.2% | +0.1% | 0.88x |
| Ordinary hash lookup | 27.10 | 23.20 | 23.12 | 23.06 | 22.84-23.47 | -14.7% | -0.4% | 0.81x |
| Ordinary hash update | 34.11 | 27.65 | 27.70 | 27.90 | 26.58-28.58 | -18.8% | +0.2% | 0.30x |
| Ordinary hash delete | 36.32 | 27.88 | 27.93 | 28.20 | 27.17-28.49 | -23.1% | +0.2% | 0.26x |
| Per-CPU array lookup | 4.12 | 4.12 | 4.11 | 4.11 | 4.10-4.13 | -0.2% | -0.1% | 0.93x |
| Per-CPU array update | 14.37 | 14.43 | 14.46 | 14.46 | 14.41-14.57 | +0.7% | +0.2% | 0.87x |
| Per-CPU hash lookup | 27.73 | 27.71 | 27.72 | 27.69 | 27.61-27.88 | -0.0% | +0.1% | 0.94x |
| Per-CPU hash update | 44.62 | 44.41 | 44.40 | 44.41 | 44.33-44.47 | -0.5% | -0.0% | 0.65x |
| Per-CPU hash delete | 98.72 | 98.87 | 98.94 | 99.02 | 98.57-99.16 | +0.2% | +0.1% | 0.65x |
| uprobe | 198.89 | 199.23 | 196.87 | 196.32 | 194.75-199.59 | -1.0% | -1.2% | 0.16x |
| uretprobe | 270.29 | 267.66 | 268.17 | 268.09 | 266.53-271.64 | -0.8% | +0.2% | 0.16x |
| uprobe + uretprobe | 323.76 | 324.39 | 324.97 | 324.90 | 324.29-325.88 | +0.4% | +0.2% | 0.18x |
| probe read | 4584.51 | 4580.43 | 4578.77 | 4578.53 | 4573.11-4585.48 | -0.1% | -0.0% | 0.25x |
| probe write | 4273.78 | 4274.65 | 4272.27 | 4271.43 | 4270.02-4275.39 | -0.0% | -0.1% | 0.24x |

Embedded `embed`: current 158.72, baseline 159.49 (-0.5%).

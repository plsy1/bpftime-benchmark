# 7ed339ad full ten rounds (userspace + embedded, no kernel arm)

Mean of ten raw ns/invocation samples divided by 1,000 operations (probe rows: ns/invocation). Baseline = 5466f0d (`baseline-177a1dc-jit-llvm16-20261021`); previous = db3d475f (`perf-lost-wakeup-20261023/full10-db3d475f`); both are different runs. The kernel arm was not run: "/kernel" uses the baseline run's kernel (not same-run). Array lookup rows do not consume the result and are not gain metrics.

| Operation | Baseline ns | db3d475f ns | Current ns | Current median | Samples min-max | vs. baseline | vs. db3d475f | /kernel (baseline run) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 1.17 | 1.17 | 1.17 | 1.17 | 1.16-1.17 | -0.4% | -0.5% | 0.31x |
| Ordinary array update | 11.57 | 11.57 | 11.52 | 11.50 | 11.47-11.70 | -0.4% | -0.4% | 0.88x |
| Ordinary hash lookup | 27.10 | 24.56 | 24.69 | 24.80 | 23.83-25.43 | -8.9% | +0.5% | 0.86x |
| Ordinary hash update | 34.11 | 30.15 | 30.59 | 30.79 | 27.74-33.02 | -10.3% | +1.5% | 0.33x |
| Ordinary hash delete | 36.32 | 27.79 | 27.67 | 27.41 | 27.23-28.46 | -23.8% | -0.4% | 0.26x |
| Per-CPU array lookup | 4.12 | 4.13 | 4.10 | 4.10 | 4.09-4.12 | -0.4% | -0.6% | 0.93x |
| Per-CPU array update | 14.37 | 14.33 | 14.29 | 14.24 | 14.18-14.62 | -0.5% | -0.3% | 0.86x |
| Per-CPU hash lookup | 27.73 | 27.74 | 27.69 | 27.71 | 27.59-27.78 | -0.1% | -0.2% | 0.94x |
| Per-CPU hash update | 44.62 | 44.47 | 44.55 | 44.57 | 44.42-44.59 | -0.2% | +0.2% | 0.65x |
| Per-CPU hash delete | 98.72 | 99.13 | 100.18 | 100.18 | 100.08-100.29 | +1.5% | +1.1% | 0.66x |
| uprobe | 198.89 | 201.02 | 198.40 | 198.25 | 196.50-201.20 | -0.2% | -1.3% | 0.16x |
| uretprobe | 270.29 | 269.09 | 266.67 | 266.41 | 264.59-270.30 | -1.3% | -0.9% | 0.16x |
| uprobe + uretprobe | 323.76 | 326.11 | 323.35 | 322.93 | 322.19-326.61 | -0.1% | -0.8% | 0.18x |
| probe read | 4584.51 | 4588.85 | 4576.65 | 4577.05 | 4571.79-4579.80 | -0.2% | -0.3% | 0.25x |
| probe write | 4273.78 | 4276.11 | 4272.01 | 4272.85 | 4269.91-4273.63 | -0.0% | -0.1% | 0.24x |

Embedded `embed`: current 161.12, baseline 159.49 (+1.0%).

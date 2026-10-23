# db3d475f full ten rounds (userspace + embedded, no kernel arm)

Mean of ten raw ns/invocation samples divided by 1,000 operations (probe rows: ns/invocation). Baseline = 5466f0d (`baseline-177a1dc-jit-llvm16-20261021`); previous = a90fa447 (`fork-child-binding-20261023/full10-a90fa447`); both are different runs. The kernel arm was not run: "/kernel" uses the baseline run's kernel (not same-run). Array lookup rows do not consume the result and are not gain metrics.

| Operation | Baseline ns | a90fa447 ns | Current ns | Current median | Samples min-max | vs. baseline | vs. a90fa447 | /kernel (baseline run) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 1.17 | 1.17 | 1.17 | 1.17 | 1.17-1.18 | +0.1% | +0.5% | 0.31x |
| Ordinary array update | 11.57 | 11.53 | 11.57 | 11.56 | 11.49-11.73 | +0.0% | +0.4% | 0.88x |
| Ordinary hash lookup | 27.10 | 24.75 | 24.56 | 24.81 | 23.86-25.03 | -9.4% | -0.8% | 0.86x |
| Ordinary hash update | 34.11 | 30.74 | 30.15 | 30.79 | 27.67-31.43 | -11.6% | -1.9% | 0.32x |
| Ordinary hash delete | 36.32 | 27.36 | 27.79 | 27.62 | 27.30-28.48 | -23.5% | +1.5% | 0.26x |
| Per-CPU array lookup | 4.12 | 4.11 | 4.13 | 4.11 | 4.10-4.28 | +0.2% | +0.5% | 0.94x |
| Per-CPU array update | 14.37 | 14.46 | 14.33 | 14.28 | 14.22-14.60 | -0.2% | -0.9% | 0.86x |
| Per-CPU hash lookup | 27.73 | 27.68 | 27.74 | 27.76 | 27.63-27.86 | +0.0% | +0.2% | 0.94x |
| Per-CPU hash update | 44.62 | 44.49 | 44.47 | 44.49 | 44.29-44.58 | -0.3% | -0.1% | 0.65x |
| Per-CPU hash delete | 98.72 | 100.04 | 99.13 | 99.10 | 98.82-99.55 | +0.4% | -0.9% | 0.65x |
| uprobe | 198.89 | 198.33 | 201.02 | 200.71 | 199.00-204.09 | +1.1% | +1.4% | 0.16x |
| uretprobe | 270.29 | 270.49 | 269.09 | 268.46 | 267.04-274.43 | -0.4% | -0.5% | 0.16x |
| uprobe + uretprobe | 323.76 | 324.77 | 326.11 | 325.62 | 323.82-331.46 | +0.7% | +0.4% | 0.19x |
| probe read | 4584.51 | 4580.02 | 4588.85 | 4588.61 | 4584.03-4593.87 | +0.1% | +0.2% | 0.25x |
| probe write | 4273.78 | 4275.23 | 4276.11 | 4275.76 | 4273.06-4281.68 | +0.1% | +0.0% | 0.24x |

Embedded `embed`: current 160.42, baseline 159.49 (+0.6%).

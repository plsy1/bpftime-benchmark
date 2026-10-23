# a90fa447 full ten rounds (userspace + embedded, no kernel arm)

Mean of ten raw ns/invocation samples divided by 1,000 operations (probe rows: ns/invocation). Baseline = 5466f0d (`baseline-177a1dc-jit-llvm16-20261021`); previous = 20870a03 (`perf-output-no-affinity-20261023/full10-20870a03`); both are different runs. The kernel arm was not run: "/kernel" uses the baseline run's kernel (not same-run). Array lookup rows do not consume the result and are not gain metrics.

| Operation | Baseline ns | 20870a03 ns | Current ns | Current median | Samples min-max | vs. baseline | vs. 20870a03 | /kernel (baseline run) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 1.17 | 1.17 | 1.17 | 1.17 | 1.16-1.17 | -0.3% | -0.2% | 0.31x |
| Ordinary array update | 11.57 | 11.53 | 11.53 | 11.51 | 11.48-11.59 | -0.3% | -0.0% | 0.88x |
| Ordinary hash lookup | 27.10 | 24.22 | 24.75 | 24.84 | 23.85-24.96 | -8.7% | +2.2% | 0.87x |
| Ordinary hash update | 34.11 | 29.29 | 30.74 | 30.85 | 27.51-31.48 | -9.9% | +5.0% | 0.33x |
| Ordinary hash delete | 36.32 | 27.94 | 27.36 | 27.28 | 27.14-28.34 | -24.7% | -2.1% | 0.25x |
| Per-CPU array lookup | 4.12 | 4.12 | 4.11 | 4.11 | 4.10-4.12 | -0.3% | -0.1% | 0.93x |
| Per-CPU array update | 14.37 | 14.49 | 14.46 | 14.46 | 14.40-14.51 | +0.6% | -0.3% | 0.87x |
| Per-CPU hash lookup | 27.73 | 27.77 | 27.68 | 27.70 | 27.54-27.83 | -0.2% | -0.3% | 0.94x |
| Per-CPU hash update | 44.62 | 44.55 | 44.49 | 44.50 | 44.35-44.53 | -0.3% | -0.1% | 0.65x |
| Per-CPU hash delete | 98.72 | 99.28 | 100.04 | 99.11 | 98.78-108.68 | +1.3% | +0.8% | 0.66x |
| uprobe | 198.89 | 197.39 | 198.33 | 198.39 | 197.58-199.10 | -0.3% | +0.5% | 0.16x |
| uretprobe | 270.29 | 268.86 | 270.49 | 270.28 | 269.78-271.88 | +0.1% | +0.6% | 0.16x |
| uprobe + uretprobe | 323.76 | 323.77 | 324.77 | 323.86 | 323.15-332.76 | +0.3% | +0.3% | 0.18x |
| probe read | 4584.51 | 4567.50 | 4580.02 | 4581.17 | 4570.10-4587.70 | -0.1% | +0.3% | 0.25x |
| probe write | 4273.78 | 4273.18 | 4275.23 | 4274.39 | 4272.17-4285.30 | +0.0% | +0.0% | 0.24x |

Embedded `embed`: current 160.35, baseline 159.49 (+0.5%).

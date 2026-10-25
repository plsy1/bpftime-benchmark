# df865b3e full ten rounds (userspace + embedded, no kernel arm)

Mean of ten raw ns/invocation samples divided by 1,000 operations (probe rows: ns/invocation). Baseline = 5466f0d (`baseline-177a1dc-jit-llvm16-20261021`); previous = bca2ebce (`perf-output-identity-20261024/full10-bca2ebce`); both are different runs. The kernel arm was not run: "/kernel" uses the baseline run's kernel (not same-run). Array lookup rows do not consume the result and are not gain metrics.

| Operation | Baseline ns | bca2ebce ns | Current ns | Current median | Samples min-max | vs. baseline | vs. bca2ebce | /kernel (baseline run) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 1.17 | 1.17 | 1.17 | 1.17 | 1.17-1.18 | +0.1% | +0.3% | 0.31x |
| Ordinary array update | 11.57 | 11.54 | 11.51 | 11.51 | 11.46-11.56 | -0.4% | -0.2% | 0.88x |
| Ordinary hash lookup | 27.10 | 24.62 | 23.26 | 23.31 | 22.89-23.67 | -14.2% | -5.5% | 0.81x |
| Ordinary hash update | 34.11 | 30.58 | 28.44 | 28.36 | 26.59-34.67 | -16.6% | -7.0% | 0.31x |
| Ordinary hash delete | 36.32 | 27.66 | 28.12 | 28.48 | 27.14-29.13 | -22.6% | +1.7% | 0.26x |
| Per-CPU array lookup | 4.12 | 4.11 | 4.12 | 4.11 | 4.11-4.13 | -0.0% | +0.1% | 0.93x |
| Per-CPU array update | 14.37 | 14.30 | 14.42 | 14.41 | 14.34-14.50 | +0.4% | +0.8% | 0.87x |
| Per-CPU hash lookup | 27.73 | 27.70 | 27.73 | 27.70 | 27.61-27.90 | -0.0% | +0.1% | 0.94x |
| Per-CPU hash update | 44.62 | 44.41 | 44.37 | 44.35 | 44.32-44.47 | -0.6% | -0.1% | 0.65x |
| Per-CPU hash delete | 98.72 | 98.72 | 99.36 | 99.34 | 99.26-99.50 | +0.7% | +0.6% | 0.65x |
| uprobe | 198.89 | 199.12 | 199.55 | 199.59 | 197.74-200.98 | +0.3% | +0.2% | 0.16x |
| uretprobe | 270.29 | 271.31 | 276.33 | 276.08 | 273.43-280.02 | +2.2% | +1.9% | 0.17x |
| uprobe + uretprobe | 323.76 | 324.43 | 333.54 | 332.97 | 331.53-336.43 | +3.0% | +2.8% | 0.19x |
| probe read | 4584.51 | 4593.84 | 4584.30 | 4584.28 | 4579.43-4589.28 | -0.0% | -0.2% | 0.25x |
| probe write | 4273.78 | 4273.63 | 4274.68 | 4274.69 | 4272.13-4278.05 | +0.0% | +0.0% | 0.24x |

Embedded `embed`: current 158.77, baseline 159.49 (-0.4%).

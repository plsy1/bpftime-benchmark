# 4d9b8a61 full ten rounds (userspace + embedded, no kernel arm)

Mean of ten raw ns/invocation samples divided by 1,000 operations (probe rows: ns/invocation). Baseline = 5466f0d (`baseline-177a1dc-jit-llvm16-20261021`); previous = f9d2d9fc (`realprog-next-20261025/full10-f9d2d9fc`); both are different runs. The kernel arm was not run: "/kernel" uses the baseline run's kernel (not same-run). Array lookup rows do not consume the result and are not gain metrics.

| Operation | Baseline ns | f9d2d9fc ns | Current ns | Current median | Samples min-max | vs. baseline | vs. f9d2d9fc | /kernel (baseline run) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 1.17 | 1.17 | 1.17 | 1.18 | 1.17-1.18 | +0.2% | +0.1% | 0.31x |
| Ordinary array update | 11.57 | 11.55 | 11.52 | 11.53 | 11.49-11.54 | -0.4% | -0.2% | 0.88x |
| Ordinary hash lookup | 27.10 | 23.12 | 23.19 | 23.06 | 22.98-23.64 | -14.4% | +0.3% | 0.81x |
| Ordinary hash update | 34.11 | 27.70 | 28.06 | 28.55 | 27.26-28.67 | -17.7% | +1.3% | 0.30x |
| Ordinary hash delete | 36.32 | 27.93 | 28.11 | 28.52 | 27.40-28.59 | -22.6% | +0.6% | 0.26x |
| Per-CPU array lookup | 4.12 | 4.11 | 4.11 | 4.11 | 4.10-4.13 | -0.2% | -0.1% | 0.93x |
| Per-CPU array update | 14.37 | 14.46 | 14.36 | 14.35 | 14.29-14.47 | -0.0% | -0.7% | 0.86x |
| Per-CPU hash lookup | 27.73 | 27.72 | 27.85 | 27.86 | 27.78-27.90 | +0.4% | +0.5% | 0.95x |
| Per-CPU hash update | 44.62 | 44.40 | 44.52 | 44.53 | 44.48-44.56 | -0.2% | +0.3% | 0.65x |
| Per-CPU hash delete | 98.72 | 98.94 | 99.45 | 99.43 | 99.32-99.64 | +0.7% | +0.5% | 0.65x |
| uprobe | 198.89 | 196.87 | 201.22 | 201.26 | 198.52-204.73 | +1.2% | +2.2% | 0.16x |
| uretprobe | 270.29 | 268.17 | 273.06 | 273.06 | 270.50-277.34 | +1.0% | +1.8% | 0.17x |
| uprobe + uretprobe | 323.76 | 324.97 | 329.06 | 328.66 | 327.17-332.80 | +1.6% | +1.3% | 0.19x |
| probe read | 4584.51 | 4578.77 | 6018.31 | 6016.55 | 6011.39-6027.61 | +31.3% | +31.4% | 0.33x |
| probe write | 4273.78 | 4272.27 | 5434.81 | 5434.78 | 5432.78-5438.14 | +27.2% | +27.2% | 0.31x |

Embedded `embed`: current 157.37, baseline 159.49 (-1.3%).

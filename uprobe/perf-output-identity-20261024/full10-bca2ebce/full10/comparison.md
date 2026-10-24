# bca2ebce full ten rounds (userspace + embedded, no kernel arm)

Mean of ten raw ns/invocation samples divided by 1,000 operations (probe rows: ns/invocation). Baseline = 5466f0d (`baseline-177a1dc-jit-llvm16-20261021`); previous = 7ed339ad (`uid-gid-cache-20261024/full10-7ed339ad`); both are different runs. The kernel arm was not run: "/kernel" uses the baseline run's kernel (not same-run). Array lookup rows do not consume the result and are not gain metrics.

| Operation | Baseline ns | 7ed339ad ns | Current ns | Current median | Samples min-max | vs. baseline | vs. 7ed339ad | /kernel (baseline run) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | 1.17 | 1.17 | 1.17 | 1.17 | 1.17-1.17 | -0.2% | +0.2% | 0.31x |
| Ordinary array update | 11.57 | 11.52 | 11.54 | 11.51 | 11.50-11.70 | -0.3% | +0.2% | 0.88x |
| Ordinary hash lookup | 27.10 | 24.69 | 24.62 | 24.81 | 23.84-24.84 | -9.1% | -0.3% | 0.86x |
| Ordinary hash update | 34.11 | 30.59 | 30.58 | 31.13 | 27.80-31.60 | -10.4% | -0.1% | 0.33x |
| Ordinary hash delete | 36.32 | 27.67 | 27.66 | 27.53 | 27.22-28.54 | -23.8% | -0.0% | 0.26x |
| Per-CPU array lookup | 4.12 | 4.10 | 4.11 | 4.11 | 4.10-4.13 | -0.2% | +0.2% | 0.93x |
| Per-CPU array update | 14.37 | 14.29 | 14.30 | 14.26 | 14.21-14.60 | -0.5% | +0.0% | 0.86x |
| Per-CPU hash lookup | 27.73 | 27.69 | 27.70 | 27.70 | 27.61-27.77 | -0.1% | +0.0% | 0.94x |
| Per-CPU hash update | 44.62 | 44.55 | 44.41 | 44.42 | 44.23-44.52 | -0.5% | -0.3% | 0.65x |
| Per-CPU hash delete | 98.72 | 100.18 | 98.72 | 98.72 | 98.66-98.80 | +0.0% | -1.5% | 0.65x |
| uprobe | 198.89 | 198.40 | 199.12 | 199.12 | 197.10-202.59 | +0.1% | +0.4% | 0.16x |
| uretprobe | 270.29 | 266.67 | 271.31 | 270.83 | 269.83-275.57 | +0.4% | +1.7% | 0.17x |
| uprobe + uretprobe | 323.76 | 323.35 | 324.43 | 323.95 | 323.27-328.31 | +0.2% | +0.3% | 0.18x |
| probe read | 4584.51 | 4576.65 | 4593.84 | 4594.53 | 4589.68-4596.26 | +0.2% | +0.4% | 0.25x |
| probe write | 4273.78 | 4272.01 | 4273.63 | 4273.58 | 4271.27-4276.00 | -0.0% | +0.0% | 0.24x |

Embedded `embed`: current 162.38, baseline 159.49 (+1.8%).

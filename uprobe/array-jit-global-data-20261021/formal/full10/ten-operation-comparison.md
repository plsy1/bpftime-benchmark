# bdd9835 global data: validation

## Full ten rounds (raw ns/op)

Baseline = 177a1dc 5466f0d full10 (different run). The original suite has no global data in its map programs, so no change is expected.

| Operation | Baseline | Baseline kernel | Current | Same-run kernel | Current/kernel | vs. baseline |
|---|---:|---:|---:|---:|---:|---:|
| Ordinary array update | 11.57 | 13.09 | 11.61 | 13.08 | 0.89x | +0.3% |
| Ordinary hash lookup | 27.10 | 28.61 | 27.22 | 27.86 | 0.98x | +0.4% |
| Ordinary hash update | 34.11 | 93.18 | 35.41 | 93.92 | 0.38x | +3.8% |
| Ordinary hash delete | 36.32 | 107.95 | 36.28 | 107.02 | 0.34x | -0.1% |
| Per-CPU array update | 14.37 | 16.63 | 14.38 | 16.62 | 0.86x | +0.1% |
| Per-CPU hash lookup | 27.73 | 29.39 | 27.76 | 28.99 | 0.96x | +0.1% |
| Per-CPU hash update | 44.62 | 68.22 | 44.47 | 67.85 | 0.66x | -0.3% |
| Per-CPU hash delete | 98.72 | 151.91 | 98.82 | 151.49 | 0.65x | +0.1% |
| Ordinary array lookup (unused result; not a gain metric) | 1.17 | 3.81 | 1.17 | 3.74 | 0.31x | -0.2% |
| Per-CPU array lookup (unused result; not a gain metric) | 4.12 | 4.41 | 4.11 | 4.41 | 0.93x | -0.3% |

## Consuming: Ordinary, standard workload (6 rounds, same run)

| Case | Kernel | Baseline 5466f0d | Current bdd9835 | Current/kernel | Current vs. baseline | Current wins |
|---|---:|---:|---:|---:|---:|---:|
| control_hit | 3.76 | 1.60 | 1.60 | 0.42x | -0.1% | 5/6 |
| hit | 5.88 | 3.10 | 3.09 | 0.53x | -0.1% | 4/6 |
| control_mixed | 9.30 | 2.50 | 2.50 | 0.27x | +0.0% | 3/6 |
| mixed | 12.30 | 8.45 | 8.45 | 0.69x | +0.1% | 2/6 |

## Consuming: Per-CPU, standard workload (6 rounds, same run)

| Case | Kernel | Baseline 5466f0d | Current bdd9835 | Current/kernel | Current vs. baseline | Current wins |
|---|---:|---:|---:|---:|---:|---:|
| control_hit | 3.76 | 1.60 | 1.60 | 0.43x | -0.1% | 5/6 |
| hit | 6.89 | 5.14 | 5.19 | 0.75x | +0.9% | 1/6 |
| control_mixed | 9.32 | 2.51 | 2.50 | 0.27x | -0.1% | 5/6 |
| mixed | 12.85 | 10.65 | 10.65 | 0.83x | +0.0% | 1/6 |

## Consuming: Ordinary, libbpf-style globals workload (6 rounds, same run)

| Case | Kernel | Baseline 5466f0d | Current bdd9835 | Current/kernel | Current vs. baseline | Current wins |
|---|---:|---:|---:|---:|---:|---:|
| control_hit | 3.77 | 1.61 | 1.60 | 0.42x | -0.9% | 6/6 |
| hit | 5.89 | 21.99 | 3.09 | 0.53x | -85.9% | 6/6 |
| control_mixed | 9.31 | 2.51 | 2.50 | 0.27x | -0.6% | 6/6 |
| mixed | 12.28 | 30.83 | 8.53 | 0.69x | -72.3% | 6/6 |

## Consuming: Per-CPU, libbpf-style globals workload (6 rounds, same run)

| Case | Kernel | Baseline 5466f0d | Current bdd9835 | Current/kernel | Current vs. baseline | Current wins |
|---|---:|---:|---:|---:|---:|---:|
| control_hit | 3.76 | 1.62 | 1.60 | 0.43x | -1.0% | 6/6 |
| hit | 6.91 | 24.79 | 5.17 | 0.75x | -79.1% | 6/6 |
| control_mixed | 9.33 | 2.52 | 2.50 | 0.27x | -0.6% | 6/6 |
| mixed | 12.82 | 32.48 | 10.65 | 0.83x | -67.2% | 6/6 |

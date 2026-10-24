# Four-byte per-CPU hash workload validation on Jetson ARM64

Status: **valid matched A/B; success**

This matrix compares the equality-only baseline with the direct four-byte hash
candidate. Each configuration alternates baseline and candidate for five
rounds on CPU5 at 1.728 GHz. Every victim sample uses 3,000 invocations, 200
warmups, and 1,000 helpers per invocation.

## Per-CPU hash median net cost

| Configuration | Lookup baseline | Candidate | Change | Update baseline | Candidate | Change |
|---|---:|---:|---:|---:|---:|---:|
| 4 B key, 64 active | 92.96 ns | 66.89 ns | **-28.04%** | 105.86 ns | 79.10 ns | **-25.28%** |
| 4 B key, 512 active | 98.43 ns | 68.48 ns | **-30.43%** | 113.79 ns | 80.83 ns | **-28.97%** |
| 4 B key, 1000 active | 100.85 ns | 68.31 ns | **-32.27%** | 116.28 ns | 80.88 ns | **-30.44%** |
| 16 B key, 512 active | 161.67 ns | 161.24 ns | -0.26% | 177.11 ns | 174.65 ns | -1.39% |
| 64 B key, 512 active | 391.25 ns | 390.87 ns | -0.10% | 404.54 ns | 402.59 ns | -0.48% |

The 4 B improvement persists across low, medium, and high occupancy. The 16 B
and 64 B paths remain within 1.4%, as expected because they retain the original
Boost byte hash. Ordinary hash controls are also archived in `summary.json`;
their changes are small and inconsistent rather than following the per-CPU
gain.

`wall-raw.csv` contains all 350 parsed case rows. `raw/` retains every loader
and victim log, `environment.json` records binary hashes and all frequency
samples, and `run.py` is the exact runner.


# Per-CPU hash optimization: full uprobe regression

Status: **success**

- Started: 2026-10-12 22:37 JST
- Finished: 2026-10-12 23:52 JST
- Duration: 4,530.86 seconds
- Power mode: MAXN_SUPER
- CPU5 frequency: 1.728 GHz for every recorded sample
- Result completeness: 17 kernel + 17 BPFtime + 1 embedded VM cases
- Samples: 10 finite positive values for every case

## Change relative to the stage-1 full regression

| BPFtime userspace case | Stage 1 mean | Candidate mean | Change |
|---|---:|---:|---:|
| per-CPU hash lookup | 137,364.24 ns/inv | 132,346.63 ns/inv | **-3.65%** |
| per-CPU hash update | 221,234.15 ns/inv | 141,286.33 ns/inv | **-36.14%** |
| ordinary hash lookup control | 58,095.38 ns/inv | 58,729.82 ns/inv | +1.09% |
| ordinary hash update control | 63,268.16 ns/inv | 63,019.05 ns/inv | -0.39% |
| per-CPU array lookup control | 21,683.41 ns/inv | 21,468.53 ns/inv | -0.99% |
| per-CPU array update control | 27,351.71 ns/inv | 27,052.91 ns/inv | -1.09% |

The complete official output is under `full/`. The root directory also contains
the final one-round matched preflight, environment record, binary hashes,
source diff, and both CMake caches.

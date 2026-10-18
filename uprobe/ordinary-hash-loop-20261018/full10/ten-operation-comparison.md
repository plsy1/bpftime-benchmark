# Full ten-round map-operation comparison

Raw average ns per invocation divided by 1000 helpers; lower is better.
This differs from the control-subtracted short A/B measurements.

| Operation | Status | Initial ns | Initial/kernel | Previous ns | Current ns | Same-run kernel ns | Current/kernel | vs. initial | vs. previous |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | Optimized | 15.94 | 4.22× | 7.95 | 8.01 | 3.81 | 2.10× | -49.8% | +0.7% |
| Ordinary array update | Optimized | 23.51 | 1.78× | 11.57 | 11.67 | 13.21 | 0.88× | -50.4% | +0.8% |
| Ordinary hash lookup | Optimized | 58.47 | 2.04× | 28.12 | 26.86 | 29.12 | 0.92× | -54.1% | -4.5% |
| Ordinary hash update | Already faster | 63.18 | 0.67× | 35.45 | 34.32 | 94.55 | 0.36× | -45.7% | -3.2% |
| Ordinary hash delete | Already faster | 42.64 | 0.40× | 36.44 | 36.83 | 108.76 | 0.34× | -13.6% | +1.1% |
| Per-CPU array lookup | Optimized | 27.73 | 6.51× | 11.65 | 11.77 | 4.41 | 2.67× | -57.5% | +1.0% |
| Per-CPU array update | Optimized | 75.93 | 4.55× | 14.32 | 14.45 | 16.59 | 0.87× | -81.0% | +0.9% |
| Per-CPU hash lookup | Optimized | 141.25 | 4.97× | 34.92 | 34.95 | 29.30 | 1.19× | -75.3% | +0.1% |
| Per-CPU hash update | Optimized | 220.96 | 3.26× | 44.46 | 44.40 | 68.60 | 0.65× | -79.9% | -0.1% |
| Per-CPU hash delete | Optimized | 986.68 | 6.50× | 98.95 | 98.55 | 152.72 | 0.65× | -90.0% | -0.4% |

Sources:

- initial: [uprobe-top-arm64-20261007-strict-repro-official-no-btf](../../uprobe-top-arm64-20261007-strict-repro-official-no-btf/benchmark-output.json)

- previous: [percpu-hash-flatten-full10-arm64-20261018](../../percpu-hash-flatten-full10-arm64-20261018/benchmark-output.json)

- current: [full10](../../ordinary-hash-loop-20261018/full10/benchmark-output.json)

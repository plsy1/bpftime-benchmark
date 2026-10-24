# Full ten-round map-operation comparison

Raw average ns per invocation divided by 1000 helpers; lower is better.
This differs from control-subtracted short A/B timings.

| Operation | Status | Initial ns | Initial/kernel | Previous ns | Current ns | Same-run kernel ns | Current/kernel | vs. initial | vs. previous |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | Optimized | 15.94 | 4.22× | 7.47 | 6.77 | 3.74 | 1.81× | -57.5% | -9.4% |
| Ordinary array update | Optimized | 23.51 | 1.78× | 11.55 | 11.54 | 13.09 | 0.88× | -50.9% | -0.0% |
| Ordinary hash lookup | Optimized | 58.47 | 2.04× | 27.07 | 27.11 | 28.28 | 0.96× | -53.6% | +0.1% |
| Ordinary hash update | Already faster | 63.18 | 0.67× | 34.42 | 33.86 | 93.29 | 0.36× | -46.4% | -1.7% |
| Ordinary hash delete | Already faster | 42.64 | 0.40× | 36.58 | 36.67 | 107.68 | 0.34× | -14.0% | +0.2% |
| Per-CPU array lookup | Optimized | 27.73 | 6.51× | 10.76 | 10.76 | 4.22 | 2.55× | -61.2% | -0.0% |
| Per-CPU array update | Optimized | 75.93 | 4.55× | 14.40 | 14.53 | 16.65 | 0.87× | -80.9% | +0.9% |
| Per-CPU hash lookup | Optimized | 141.25 | 4.97× | 27.72 | 27.71 | 29.47 | 0.94× | -80.4% | -0.1% |
| Per-CPU hash update | Optimized | 220.96 | 3.26× | 44.39 | 44.38 | 67.84 | 0.65× | -79.9% | -0.0% |
| Per-CPU hash delete | Optimized | 986.68 | 6.50× | 98.70 | 98.70 | 151.52 | 0.65× | -90.0% | +0.0% |

Sources:

- initial: [uprobe-top-arm64-20261007-strict-repro-official-no-btf](../../uprobe-top-arm64-20261007-strict-repro-official-no-btf/benchmark-output.json)
- previous: [full10](../../ordinary-array-lookup-cold-20261019/full10/benchmark-output.json)
- current: [full10](../../array-offset-branch-20261019/full10/benchmark-output.json)

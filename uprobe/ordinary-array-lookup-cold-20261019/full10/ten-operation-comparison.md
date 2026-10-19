# Full ten-round map-operation comparison

Raw average ns per invocation divided by 1000 helpers; lower is better.
This differs from control-subtracted short A/B timings.

| Operation | Status | Initial ns | Initial/kernel | Previous ns | Current ns | Same-run kernel ns | Current/kernel | vs. initial | vs. previous |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | Optimized | 15.94 | 4.22× | 7.93 | 7.47 | 3.81 | 1.96× | -53.1% | -5.8% |
| Ordinary array update | Optimized | 23.51 | 1.78× | 11.54 | 11.55 | 13.08 | 0.88× | -50.9% | +0.1% |
| Ordinary hash lookup | Optimized | 58.47 | 2.04× | 26.99 | 27.07 | 28.94 | 0.94× | -53.7% | +0.3% |
| Ordinary hash update | Already faster | 63.18 | 0.67× | 35.02 | 34.42 | 94.18 | 0.37× | -45.5% | -1.7% |
| Ordinary hash delete | Already faster | 42.64 | 0.40× | 36.51 | 36.58 | 108.78 | 0.34× | -14.2% | +0.2% |
| Per-CPU array lookup | Optimized | 27.73 | 6.51× | 10.92 | 10.76 | 4.40 | 2.45× | -61.2% | -1.5% |
| Per-CPU array update | Optimized | 75.93 | 4.55× | 14.44 | 14.40 | 16.62 | 0.87× | -81.0% | -0.3% |
| Per-CPU hash lookup | Optimized | 141.25 | 4.97× | 27.70 | 27.72 | 28.95 | 0.96× | -80.4% | +0.1% |
| Per-CPU hash update | Optimized | 220.96 | 3.26× | 44.55 | 44.39 | 67.97 | 0.65× | -79.9% | -0.4% |
| Per-CPU hash delete | Optimized | 986.68 | 6.50× | 99.06 | 98.70 | 150.96 | 0.65× | -90.0% | -0.4% |

Sources:

- initial: [uprobe-top-arm64-20261007-strict-repro-official-no-btf](../../uprobe-top-arm64-20261007-strict-repro-official-no-btf/benchmark-output.json)
- previous: [full10](../../percpu-array-lookup-followup-20261018/full10/benchmark-output.json)
- current: [full10](../../ordinary-array-lookup-cold-20261019/full10/benchmark-output.json)

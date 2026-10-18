# Full ten-round map-operation comparison

Raw average ns per invocation divided by 1000 helpers; lower is better.
This differs from the control-subtracted short A/B measurements.

| Operation | Status | Initial ns | Initial/kernel | Previous ns | Current ns | Same-run kernel ns | Current/kernel | vs. initial | vs. previous |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | Optimized | 15.94 | 4.22× | 8.01 | 7.93 | 3.80 | 2.08× | -50.3% | -1.0% |
| Ordinary array update | Optimized | 23.51 | 1.78× | 11.67 | 11.55 | 13.21 | 0.87× | -50.9% | -1.0% |
| Ordinary hash lookup | Optimized | 58.47 | 2.04× | 26.86 | 26.97 | 28.19 | 0.96× | -53.9% | +0.4% |
| Ordinary hash update | Already faster | 63.18 | 0.67× | 34.32 | 34.92 | 95.54 | 0.37× | -44.7% | +1.7% |
| Ordinary hash delete | Already faster | 42.64 | 0.40× | 36.83 | 36.52 | 107.86 | 0.34× | -14.3% | -0.8% |
| Per-CPU array lookup | Optimized | 27.73 | 6.51× | 11.77 | 11.64 | 4.41 | 2.64× | -58.0% | -1.1% |
| Per-CPU array update | Optimized | 75.93 | 4.55× | 14.45 | 14.32 | 16.60 | 0.86× | -81.1% | -0.9% |
| Per-CPU hash lookup | Optimized | 141.25 | 4.97× | 34.95 | 27.82 | 29.97 | 0.93× | -80.3% | -20.4% |
| Per-CPU hash update | Optimized | 220.96 | 3.26× | 44.40 | 44.59 | 68.23 | 0.65× | -79.8% | +0.4% |
| Per-CPU hash delete | Optimized | 986.68 | 6.50× | 98.55 | 98.96 | 151.58 | 0.65× | -90.0% | +0.4% |

Sources:

- initial: [uprobe-top-arm64-20261007-strict-repro-official-no-btf](../../uprobe-top-arm64-20261007-strict-repro-official-no-btf/benchmark-output.json)

- previous: [full10](../../ordinary-hash-loop-20261018/full10/benchmark-output.json)

- current: [full10](../../percpu-hash-lookup-followup-20261018/full10/benchmark-output.json)

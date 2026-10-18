# Full ten-round map-operation comparison

Raw average ns per invocation divided by 1000 helpers; lower is better.
This differs from the control-subtracted short A/B measurements.

| Operation | Status | Initial ns | Initial/kernel | Previous ns | Current ns | Same-run kernel ns | Current/kernel | vs. initial | vs. previous |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Ordinary array lookup | Optimized | 15.94 | 4.22× | 7.93 | 7.93 | 3.80 | 2.09× | -50.2% | +0.1% |
| Ordinary array update | Optimized | 23.51 | 1.78× | 11.55 | 11.54 | 13.08 | 0.88× | -50.9% | -0.1% |
| Ordinary hash lookup | Optimized | 58.47 | 2.04× | 26.97 | 26.99 | 28.36 | 0.95× | -53.8% | +0.0% |
| Ordinary hash update | Already faster | 63.18 | 0.67× | 34.92 | 35.02 | 94.02 | 0.37× | -44.6% | +0.3% |
| Ordinary hash delete | Already faster | 42.64 | 0.40× | 36.52 | 36.51 | 107.94 | 0.34× | -14.4% | -0.0% |
| Per-CPU array lookup | Optimized | 27.73 | 6.51× | 11.64 | 10.92 | 4.41 | 2.48× | -60.6% | -6.2% |
| Per-CPU array update | Optimized | 75.93 | 4.55× | 14.32 | 14.44 | 16.61 | 0.87× | -81.0% | +0.8% |
| Per-CPU hash lookup | Optimized | 141.25 | 4.97× | 27.82 | 27.70 | 29.16 | 0.95× | -80.4% | -0.4% |
| Per-CPU hash update | Optimized | 220.96 | 3.26× | 44.59 | 44.55 | 68.26 | 0.65× | -79.8% | -0.1% |
| Per-CPU hash delete | Optimized | 986.68 | 6.50× | 98.96 | 99.06 | 150.98 | 0.66× | -90.0% | +0.1% |

Sources:

- initial: [uprobe-top-arm64-20261007-strict-repro-official-no-btf](../../uprobe-top-arm64-20261007-strict-repro-official-no-btf/benchmark-output.json)

- previous: [full10](../../percpu-hash-lookup-followup-20261018/full10/benchmark-output.json)

- current: [full10](../../percpu-array-lookup-followup-20261018/full10/benchmark-output.json)

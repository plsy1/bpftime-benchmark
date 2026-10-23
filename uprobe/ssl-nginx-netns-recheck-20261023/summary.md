payload 1024 B, 9 complete reps (excluded: [2])

| Leg | private req/s | host req/s | host vs private (95% CI) | reader cores p/h | nginx cores p/h |
|---|---:|---:|---:|---:|---:|
| baseline | 16518.1 | 15237.8 | -7.66% [-8.29, -7.03] | 0.000/0.000 | 0.857/0.844 |
| kernel | 11458.4 | 10417.7 | -9.11% [-10.78, -7.44] | 0.111/0.107 | 0.853/0.842 |
| official | 10193.5 | 9644.2 | -6.52% [-8.29, -4.75] | 0.053/0.050 | 0.912/0.901 |
| opt-noaff-fork | 12345.7 | 11409.2 | -8.14% [-9.77, -6.51] | 0.059/0.055 | 0.882/0.867 |

| Tracer | vs kernel, private | vs kernel, host | interaction vs kernel (95% CI) |
|---|---:|---:|---:|
| official | -11.21% | -8.97% | +2.58pp [-0.08, +5.25] |
| opt-noaff-fork | +8.10% | +9.17% | +0.97pp [-0.88, +2.81] |

kernel vs baseline interaction: -1.44pp [-3.30, +0.41]
host INPUT packets/request: 2.03; repeated legs: 3; foreign seen in kept legs: 1

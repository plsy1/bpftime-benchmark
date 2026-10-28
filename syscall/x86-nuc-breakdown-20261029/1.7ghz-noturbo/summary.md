| Leg | runs | median ns/write | min-max | step | over none |
|---|---:|---:|---|---:|---:|
| none | 5 | 234.0 | 233.3-242.9 | - | - |
| rewrite | 5 | 299.8 | 299.1-300.9 | +65.8 (vs none) | +65.8 |
| passthrough | 5 | 300.6 | 299.9-307.0 | +0.8 (vs rewrite) | +66.6 |
| agent-other | 5 | 430.2 | 429.3-431.1 | +129.6 (vs passthrough) | +196.2 |
| agent-exit | 5 | 459.7 | 458.5-460.8 | +29.5 (vs agent-other) | +225.7 |
| agent-count | 5 | 490.1 | 489.4-494.8 | +30.4 (vs agent-exit) | +256.1 |
| kernel-other | 5 | 270.7 | 270.6-282.7 | +36.7 (vs none) | +36.7 |
| kernel-exit | 5 | 286.0 | 285.6-295.2 | +15.3 (vs kernel-other) | +52.0 |
| kernel-count | 5 | 429.1 | 427.3-435.6 | +143.1 (vs kernel-exit) | +195.1 |

runs 45, not ok 0

| Leg | runs | median ns/write | min-max | step | over none |
|---|---:|---:|---|---:|---:|
| none | 5 | 352.4 | 352.2-353.3 | - | - |
| rewrite | 5 | 389.9 | 389.4-390.2 | +37.5 (vs none) | +37.5 |
| passthrough | 5 | 391.0 | 390.7-396.4 | +1.1 (vs rewrite) | +38.6 |
| agent-other | 5 | 418.8 | 416.1-420.6 | +27.8 (vs passthrough) | +66.4 |
| agent-exit | 5 | 498.5 | 493.7-531.2 | +79.7 (vs agent-other) | +146.1 |
| agent-count | 5 | 600.0 | 590.9-612.7 | +101.5 (vs agent-exit) | +247.6 |
| kernel-other | 5 | 367.8 | 367.3-368.1 | +15.4 (vs none) | +15.4 |
| kernel-exit | 5 | 438.1 | 436.1-444.9 | +70.3 (vs kernel-other) | +85.7 |
| kernel-count | 5 | 524.3 | 522.6-538.5 | +86.2 (vs kernel-exit) | +171.9 |

runs 45, not ok 0

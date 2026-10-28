| Leg | runs | median ns/write | min-max | step | over none |
|---|---:|---:|---|---:|---:|
| none | 5 | 352.6 | 352.2-353.0 | - | - |
| rewrite | 5 | 390.1 | 389.8-390.6 | +37.5 (vs none) | +37.5 |
| passthrough | 5 | 390.5 | 389.8-390.9 | +0.4 (vs rewrite) | +37.9 |
| agent-other | 5 | 592.6 | 585.2-603.8 | +202.1 (vs passthrough) | +240.0 |
| agent-exit | 5 | 634.8 | 628.9-648.3 | +42.2 (vs agent-other) | +282.2 |
| agent-count | 5 | 681.8 | 679.1-686.3 | +47.0 (vs agent-exit) | +329.2 |
| kernel-other | 5 | 367.4 | 366.6-367.5 | +14.8 (vs none) | +14.8 |
| kernel-exit | 5 | 436.7 | 435.6-437.1 | +69.3 (vs kernel-other) | +84.1 |
| kernel-count | 5 | 523.6 | 522.1-529.1 | +86.9 (vs kernel-exit) | +171.0 |

runs 45, not ok 0

| Leg | runs | median ns/write | min-max | step | over none |
|---|---:|---:|---|---:|---:|
| none | 5 | 91.2 | 90.0-95.1 | - | - |
| rewrite | 5 | 116.3 | 115.5-118.1 | +25.1 (vs none) | +25.1 |
| passthrough | 5 | 116.6 | 115.3-118.9 | +0.3 (vs rewrite) | +25.4 |
| agent-other | 5 | 125.5 | 125.2-126.3 | +8.9 (vs passthrough) | +34.3 |
| agent-exit | 5 | 151.0 | 150.0-151.8 | +25.5 (vs agent-other) | +59.8 |
| agent-count | 5 | 177.2 | 176.8-179.5 | +26.2 (vs agent-exit) | +86.0 |
| kernel-other | 5 | 107.4 | 106.0-112.8 | +16.2 (vs none) | +16.2 |
| kernel-exit | 5 | 112.7 | 110.7-114.9 | +5.3 (vs kernel-other) | +21.5 |
| kernel-count | 5 | 169.7 | 166.5-170.9 | +57.0 (vs kernel-exit) | +78.5 |

runs 45, not ok 0

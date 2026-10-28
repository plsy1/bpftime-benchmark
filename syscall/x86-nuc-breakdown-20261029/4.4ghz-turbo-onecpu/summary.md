| Leg | runs | median ns/write | min-max | step | over none |
|---|---:|---:|---|---:|---:|
| none | 5 | 91.0 | 90.1-95.3 | - | - |
| rewrite | 5 | 115.9 | 115.0-116.7 | +24.9 (vs none) | +24.9 |
| passthrough | 5 | 116.1 | 115.6-118.2 | +0.2 (vs rewrite) | +25.1 |
| agent-other | 5 | 166.3 | 166.0-174.5 | +50.2 (vs passthrough) | +75.3 |
| agent-exit | 5 | 177.0 | 176.8-178.3 | +10.7 (vs agent-other) | +86.0 |
| agent-count | 5 | 190.2 | 188.6-201.1 | +13.2 (vs agent-exit) | +99.2 |
| kernel-other | 5 | 105.9 | 104.6-106.2 | +14.9 (vs none) | +14.9 |
| kernel-exit | 5 | 111.4 | 110.5-113.4 | +5.5 (vs kernel-other) | +20.4 |
| kernel-count | 5 | 166.5 | 164.7-167.5 | +55.1 (vs kernel-exit) | +75.5 |

runs 45, not ok 0

| Leg | runs | median ns/write | min-max | step | over none |
|---|---:|---:|---|---:|---:|
| none | 5 | 91.7 | 90.8-93.1 | - | - |
| rewrite | 5 | 116.2 | 115.5-127.9 | +24.5 (vs none) | +24.5 |
| passthrough | 5 | 116.2 | 115.9-116.8 | +0.0 (vs rewrite) | +24.5 |
| agent-other | 5 | 167.2 | 166.3-172.4 | +51.0 (vs passthrough) | +75.5 |
| agent-exit | 5 | 178.3 | 177.6-183.3 | +11.1 (vs agent-other) | +86.6 |
| agent-count | 5 | 190.3 | 189.2-210.1 | +12.0 (vs agent-exit) | +98.6 |
| kernel-other | 5 | 106.6 | 106.0-107.6 | +14.9 (vs none) | +14.9 |
| kernel-exit | 5 | 113.7 | 110.9-116.9 | +7.1 (vs kernel-other) | +22.0 |
| kernel-count | 5 | 166.9 | 165.5-169.3 | +53.2 (vs kernel-exit) | +75.2 |

runs 45, not ok 0

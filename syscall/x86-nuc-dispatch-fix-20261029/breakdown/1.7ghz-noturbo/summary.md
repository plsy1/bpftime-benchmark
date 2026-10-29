| Leg | runs | median ns/write | min-max | step | over none |
|---|---:|---:|---|---:|---:|
| none | 5 | 234.3 | 233.7-235.1 | - | - |
| rewrite | 5 | 298.8 | 298.3-309.7 | +64.5 (vs none) | +64.5 |
| passthrough | 5 | 298.7 | 297.9-301.2 | +-0.1 (vs rewrite) | +64.4 |
| agent-other | 5 | 322.9 | 322.6-323.3 | +24.2 (vs passthrough) | +88.6 |
| agent-exit | 5 | 388.2 | 387.1-389.3 | +65.3 (vs agent-other) | +153.9 |
| agent-count | 5 | 458.5 | 456.1-469.6 | +70.3 (vs agent-exit) | +224.2 |
| kernel-other | 5 | 271.0 | 270.1-271.8 | +36.7 (vs none) | +36.7 |
| kernel-exit | 5 | 285.9 | 285.1-292.0 | +14.9 (vs kernel-other) | +51.6 |
| kernel-count | 5 | 428.2 | 424.9-435.2 | +142.3 (vs kernel-exit) | +193.9 |

runs 45, not ok 0

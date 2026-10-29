Median ns per write per run, and the layer over none (cycles at 1709 MHz).

| Leg | run1-59e5e154 | run2-c23442ac | run3-59e5e154 | run4-c23442ac |
|---|---:|---:|---:|---:|
| none | 234.8 | 233.4 | 235.1 | 234.1 |
| rewrite | 299.8 (+65.0, 111 cyc) | 450.0 (+216.6, 370 cyc) | 300.0 (+64.9, 111 cyc) | 449.7 (+215.6, 368 cyc) |
| passthrough | 300.3 (+65.5, 112 cyc) | 450.4 (+217.0, 371 cyc) | 299.2 (+64.1, 110 cyc) | 449.8 (+215.7, 369 cyc) |
| agent-other | 324.0 (+89.2, 152 cyc) | 474.0 (+240.6, 411 cyc) | 323.5 (+88.4, 151 cyc) | 475.0 (+240.9, 412 cyc) |
| agent-exit | 388.8 (+154.0, 263 cyc) | 535.7 (+302.3, 517 cyc) | 388.7 (+153.6, 263 cyc) | 534.3 (+300.2, 513 cyc) |
| agent-count | 455.7 (+220.9, 378 cyc) | 603.9 (+370.5, 633 cyc) | 456.0 (+220.9, 378 cyc) | 603.2 (+369.1, 631 cyc) |

Fix cost (mean of the two runs per build, over none):

| Layer | 59e5e154 | c23442ac | difference |
|---|---:|---:|---:|
| rewrite − none | 65.0 ns (111 cyc) | 216.1 ns (369 cyc) | **+151.1 ns (+258 cyc)** |
| agent-other − none | 88.8 ns (152 cyc) | 240.8 ns (411 cyc) | **+151.9 ns (+260 cyc)** |
| agent-exit − none | 153.8 ns (263 cyc) | 301.2 ns (515 cyc) | **+147.4 ns (+252 cyc)** |
| agent-count − none | 220.9 ns (378 cyc) | 369.8 ns (632 cyc) | **+148.9 ns (+254 cyc)** |

Median ns per write (5 rounds), in brackets the step over the layer below and the step in cycles.

### 1.7 GHz (x86) and Jetson 1.728 GHz

| Leg | x86 before | x86 after | Jetson before | Jetson after |
|---|---:|---:|---:|---:|
| none | 234.0 | 234.3 | 352.6 | 352.4 |
| rewrite | 299.8 (+65.8 ns, +112 cyc) | 298.8 (+64.5 ns, +110 cyc) | 390.1 (+37.5 ns, +65 cyc) | 389.9 (+37.5 ns, +65 cyc) |
| passthrough | 300.6 (+0.8 ns, +1 cyc) | 298.7 (-0.1 ns, -0 cyc) | 390.5 (+0.4 ns, +1 cyc) | 391.0 (+1.1 ns, +2 cyc) |
| agent-other | 430.2 (+129.6 ns, +221 cyc) | 322.9 (+24.2 ns, +41 cyc) | 592.6 (+202.1 ns, +349 cyc) | 418.8 (+27.8 ns, +48 cyc) |
| agent-exit | 459.7 (+29.5 ns, +50 cyc) | 388.2 (+65.3 ns, +112 cyc) | 634.8 (+42.2 ns, +73 cyc) | 498.5 (+79.7 ns, +138 cyc) |
| agent-count | 490.1 (+30.4 ns, +52 cyc) | 458.5 (+70.3 ns, +120 cyc) | 681.8 (+47.0 ns, +81 cyc) | 600.0 (+101.5 ns, +175 cyc) |
| kernel-other | 270.7 (+36.7 ns, +63 cyc) | 271.0 (+36.7 ns, +63 cyc) | 367.4 (+14.8 ns, +26 cyc) | 367.8 (+15.4 ns, +27 cyc) |
| kernel-exit | 286.0 (+15.3 ns, +26 cyc) | 285.9 (+14.9 ns, +25 cyc) | 436.7 (+69.3 ns, +120 cyc) | 438.1 (+70.3 ns, +121 cyc) |
| kernel-count | 429.1 (+143.1 ns, +245 cyc) | 428.2 (+142.3 ns, +243 cyc) | 523.6 (+86.9 ns, +150 cyc) | 524.3 (+86.2 ns, +149 cyc) |
| bpftime agent-exit over none | +225.7 ns, 386 cyc | +153.9 ns, 263 cyc | +282.2 ns, 488 cyc | +146.1 ns, 252 cyc |

### 4.4 GHz (x86)

| Leg | x86 before | x86 after |
|---|---:|---:|
| none | 91.7 | 91.2 |
| rewrite | 116.2 (+24.5 ns, +107 cyc) | 116.3 (+25.1 ns, +109 cyc) |
| passthrough | 116.2 (+0.0 ns, +0 cyc) | 116.6 (+0.3 ns, +1 cyc) |
| agent-other | 167.2 (+51.0 ns, +222 cyc) | 125.5 (+8.9 ns, +39 cyc) |
| agent-exit | 178.3 (+11.1 ns, +48 cyc) | 151.0 (+25.5 ns, +111 cyc) |
| agent-count | 190.3 (+12.0 ns, +52 cyc) | 177.2 (+26.2 ns, +114 cyc) |
| kernel-other | 106.6 (+14.9 ns, +65 cyc) | 107.4 (+16.2 ns, +70 cyc) |
| kernel-exit | 113.7 (+7.1 ns, +31 cyc) | 112.7 (+5.3 ns, +23 cyc) |
| kernel-count | 166.9 (+53.2 ns, +232 cyc) | 169.7 (+57.0 ns, +248 cyc) |
| bpftime agent-exit over none | +86.6 ns, 377 cyc | +59.8 ns, 260 cyc |

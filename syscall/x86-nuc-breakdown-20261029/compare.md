ns per write (median of 5), step over the layer below, and the step in cycles

| Leg | Jetson 1.73 GHz | x86 1.7 GHz | x86 4.4 GHz | x86 4.4 GHz (1st) |
|---|---:|---:|---:|---:|
| none | 352.6 | 234.0 | 91.7 | 91.0 |
| rewrite | 390.1 (+37.5 ns, +65 cyc) | 299.8 (+65.8 ns, +112 cyc) | 116.2 (+24.5 ns, +107 cyc) | 115.9 (+24.9 ns, +110 cyc) |
| passthrough | 390.5 (+0.4 ns, +1 cyc) | 300.6 (+0.8 ns, +1 cyc) | 116.2 (+0.0 ns, +0 cyc) | 116.1 (+0.2 ns, +1 cyc) |
| agent-other | 592.6 (+202.1 ns, +349 cyc) | 430.2 (+129.6 ns, +221 cyc) | 167.2 (+51.0 ns, +222 cyc) | 166.3 (+50.2 ns, +221 cyc) |
| agent-exit | 634.8 (+42.2 ns, +73 cyc) | 459.7 (+29.5 ns, +50 cyc) | 178.3 (+11.1 ns, +48 cyc) | 177.0 (+10.7 ns, +47 cyc) |
| agent-count | 681.8 (+47.0 ns, +81 cyc) | 490.1 (+30.4 ns, +52 cyc) | 190.3 (+12.0 ns, +52 cyc) | 190.2 (+13.2 ns, +58 cyc) |
| kernel-other | 367.4 (+14.8 ns, +26 cyc) | 270.7 (+36.7 ns, +63 cyc) | 106.6 (+14.9 ns, +65 cyc) | 105.9 (+14.9 ns, +66 cyc) |
| kernel-exit | 436.7 (+69.3 ns, +120 cyc) | 286.0 (+15.3 ns, +26 cyc) | 113.7 (+7.1 ns, +31 cyc) | 111.4 (+5.5 ns, +24 cyc) |
| kernel-count | 523.6 (+86.9 ns, +150 cyc) | 429.1 (+143.1 ns, +245 cyc) | 166.9 (+53.2 ns, +232 cyc) | 166.5 (+55.1 ns, +242 cyc) |

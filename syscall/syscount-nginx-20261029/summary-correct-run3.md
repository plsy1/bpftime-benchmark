| Leg | runs | requests/s mean | sd | min-max | vs no tracing | +us per request | +ns per nginx system call | calls counted per request |
|---|---:|---:|---:|---|---:|---:|---:|---|
| native | 10 | 27928 | 241 | 27555-28321 | +0.00% | +0.00 | +0 | - |
| kernel_targeted | 10 | 26352 | 377 | 25587-26875 | -5.64% | +2.14 | +237 | 9.018-9.019 |
| kernel_untargeted | 10 | 26918 | 248 | 26315-27230 | -3.61% | +1.34 | +149 | 0.000-0.000 |
| bpftime_targeted | 10 | 24746 | 252 | 24448-25148 | -11.39% | +4.60 | +510 | 9.018-9.019 |
| bpftime_untargeted | 10 | 25411 | 211 | 25134-25839 | -9.01% | +3.55 | +393 | 0.000-0.000 |
| transformer_only | 10 | 26983 | 334 | 26473-27626 | -3.38% | +1.25 | +139 | - |

runs not valid: 0; nginx system calls per request (kernel count): 9.019

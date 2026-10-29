| Leg | runs | requests/s mean | sd | min-max | vs no tracing | +us per request | +ns per nginx system call | calls counted per request |
|---|---:|---:|---:|---|---:|---:|---:|---|
| native | 10 | 58371 | 229 | 58033-58856 | +0.00% | +0.00 | +0 | - |
| kernel_targeted | 10 | 55273 | 235 | 54808-55559 | -5.31% | +0.96 | +107 | 9.015-9.015 |
| kernel_untargeted | 10 | 56251 | 318 | 55707-56782 | -3.63% | +0.65 | +72 | 0.000-0.001 |
| bpftime_targeted | 10 | 47466 | 262 | 47100-47720 | -18.68% | +3.94 | +437 | 9.015-9.015 |
| bpftime_untargeted | 10 | 48383 | 335 | 47602-48798 | -17.11% | +3.54 | +392 | 0.000-0.000 |
| transformer_only | 10 | 51410 | 145 | 51219-51620 | -11.92% | +2.32 | +257 | - |

runs not valid: 0; nginx system calls per request (kernel count): 9.015

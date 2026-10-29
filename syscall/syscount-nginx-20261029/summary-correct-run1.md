| Leg | runs | requests/s mean | sd | min-max | vs no tracing | +us per request | +ns per nginx system call | calls counted per request |
|---|---:|---:|---:|---|---:|---:|---:|---|
| native | 10 | 24469 | 425 | 23817-25152 | +0.00% | +0.00 | +0 | - |
| kernel_targeted | 10 | 23345 | 446 | 22924-24061 | -4.59% | +1.97 | +218 | 9.019-9.020 |
| kernel_untargeted | 10 | 23698 | 521 | 23020-24597 | -3.15% | +1.33 | +148 | 0.000-0.002 |
| bpftime_targeted | 10 | 21788 | 367 | 21305-22352 | -10.96% | +5.03 | +558 | 9.019-9.020 |
| bpftime_untargeted | 10 | 22151 | 562 | 21329-22961 | -9.47% | +4.28 | +474 | 0.000-0.000 |
| transformer_only | 10 | 23820 | 488 | 23085-24800 | -2.65% | +1.11 | +123 | - |

runs not valid: 0; nginx system calls per request (kernel count): 9.020

| Configuration | runs | requests/s mean | sd | min-max | vs no tracing |
|---|---:|---:|---:|---|---:|
| No tracing | 10 | 53884 | 171 | 53571-54121 | - |
| Kernel syscount, nginx | 10 | 51583 | 198 | 51172-51819 | -4.27% |
| Kernel syscount, other process (-p 1) | 10 | 52312 | 216 | 51945-52595 | -2.92% |
| bpftime syscount, nginx (traces nothing) | 10 | 53306 | 299 | 52943-53915 | -1.07% |
| bpftime syscount, other process (traces nothing) | 10 | 53037 | 254 | 52721-53446 | -1.57% |

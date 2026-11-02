
### count (us per op per thread, median of 3; runs 216, not ok 0)

| config | kernel | dev-42595149 | dev-e5ba4aa7 | step2/step1 |
|---|---:|---:|---:|---:|
| count-control.bpf.o 1 1 | 1.246 | 0.211 | 0.210 | 1.00x |
| count-control.bpf.o 1 2 | 1.848 | 0.277 | 0.278 | 1.00x |
| count-control.bpf.o 1 4 | 3.506 | 2.311 | 2.326 | 1.01x |
| count-control.bpf.o 1 6 | 4.117 | 4.738 | 4.681 | 0.99x |
| count-control.bpf.o 64 1 | 1.237 | 0.211 | 0.210 | 1.00x |
| count-control.bpf.o 64 2 | 1.845 | 0.279 | 0.275 | 0.99x |
| count-control.bpf.o 64 4 | 3.302 | 2.257 | 2.325 | 1.03x |
| count-control.bpf.o 64 6 | 4.101 | 4.711 | 4.729 | 1.00x |
| count-hash.bpf.o 1 1 | 1.262 | 0.236 | 0.241 | 1.02x |
| count-hash.bpf.o 1 2 | 1.877 | 0.394 | 0.392 | 1.00x |
| count-hash.bpf.o 1 4 | 3.288 | 2.254 | 2.369 | 1.05x |
| count-hash.bpf.o 1 6 | 4.093 | 4.672 | 4.696 | 1.00x |
| count-hash.bpf.o 64 1 | 1.261 | 0.242 | 0.239 | 0.99x |
| count-hash.bpf.o 64 2 | 1.842 | 0.398 | 0.342 | 0.86x |
| count-hash.bpf.o 64 4 | 3.385 | 2.310 | 2.392 | 1.04x |
| count-hash.bpf.o 64 6 | 4.115 | 4.728 | 4.750 | 1.00x |
| count-percpu-hash.bpf.o 1 1 | 1.276 | 0.242 | 0.255 | 1.05x |
| count-percpu-hash.bpf.o 1 2 | 1.844 | 0.345 | 0.356 | 1.03x |
| count-percpu-hash.bpf.o 1 4 | 3.328 | 2.284 | 2.322 | 1.02x |
| count-percpu-hash.bpf.o 1 6 | 4.086 | 4.666 | 4.682 | 1.00x |
| count-percpu-hash.bpf.o 64 1 | 1.268 | 0.247 | 0.254 | 1.03x |
| count-percpu-hash.bpf.o 64 2 | 1.839 | 0.343 | 0.350 | 1.02x |
| count-percpu-hash.bpf.o 64 4 | 3.377 | 2.306 | 2.403 | 1.04x |
| count-percpu-hash.bpf.o 64 6 | 4.118 | 4.699 | 4.695 | 1.00x |

### churn (us per op per thread, median of 3; runs 24, not ok 0)

| config | dev-42595149 | dev-e5ba4aa7 | step2/step1 |
|---|---:|---:|---:|
| churn-k4-v8.bpf.o 1 | 0.583 | 0.599 | 1.03x |
| churn-k4-v8.bpf.o 6 | 5.094 | 5.075 | 1.00x |
| control.bpf.o 1 | 0.454 | 0.451 | 0.99x |
| control.bpf.o 6 | 5.018 | 4.980 | 0.99x |

### track (us per op per thread, median of 3; runs 48, not ok 0)

| config | dev-42595149 | dev-e5ba4aa7 | step2/step1 |
|---|---:|---:|---:|
| track-control.bpf.o 1000 1 | 0.491 | 0.496 | 1.01x |
| track-control.bpf.o 1000 6 | 5.962 | 5.980 | 1.00x |
| track-control.bpf.o 900000 1 | 0.484 | 0.490 | 1.01x |
| track-control.bpf.o 900000 6 | 4.970 | 5.190 | 1.04x |
| track.bpf.o 1000 1 | 1.014 | 0.736 | 0.73x |
| track.bpf.o 1000 6 | 5.108 | 5.117 | 1.00x |
| track.bpf.o 900000 1 | 1.227 | 0.978 | 0.80x |
| track.bpf.o 900000 6 | 5.067 | 4.964 | 0.98x |

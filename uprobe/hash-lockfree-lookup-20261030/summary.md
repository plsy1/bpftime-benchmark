
Shared counters, NKEYS 1: hash map cost per call (us) | total per call (us)
| Threads | kernel map | c98951a4 map | 9c3e0345 map | kernel total | c98951a4 total | 9c3e0345 total |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.02 | 0.02 | 0.02 | 1.26 | 0.23 | 0.24 |
| 2 | 0.02 | 0.20 | 0.12 | 1.86 | 0.48 | 0.39 |
| 4 | 0.04 | 1.07 | -0.00 | 3.33 | 3.37 | 2.25 |
| 6 | 0.14 | 0.85 | -0.00 | 4.19 | 5.58 | 4.67 |

Shared counters, NKEYS 64: hash map cost per call (us) | total per call (us)
| Threads | kernel map | c98951a4 map | 9c3e0345 map | kernel total | c98951a4 total | 9c3e0345 total |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.01 | 0.03 | 0.03 | 1.26 | 0.24 | 0.24 |
| 2 | 0.05 | 0.20 | 0.12 | 1.90 | 0.48 | 0.40 |
| 4 | 0.20 | 0.83 | 0.03 | 3.55 | 3.14 | 2.30 |
| 6 | 0.06 | 0.97 | 0.02 | 4.13 | 5.72 | 4.74 |

Churn (insert at entry, lookup + delete at return), 4 B key, 8 B value: map cost | total (us)
| Threads | kernel map | c98951a4 map | 9c3e0345 map | kernel total | c98951a4 total | 9c3e0345 total |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.22 | 0.12 | 0.11 | 2.04 | 0.57 | 0.56 |
| 2 | 0.40 | 0.42 | 0.20 | 3.03 | 0.96 | 0.74 |
| 4 | 0.16 | 6.18 | 3.77 | 5.02 | 8.96 | 6.52 |
| 6 | 0.24 | 10.36 | 6.49 | 6.74 | 15.38 | 11.51 |

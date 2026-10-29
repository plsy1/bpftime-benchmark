
Churn (insert at entry, lookup + delete at return), 4 B key, 8 B value: map cost | total (us)
| Threads | kernel map | c98951a4 map | 9c3e0345 map | cb3f29d2 map | kernel total | c98951a4 total | 9c3e0345 total | cb3f29d2 total |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.26 | 0.12 | 0.11 | 0.22 | 2.09 | 0.57 | 0.56 | 0.67 |
| 2 | 0.32 | 0.43 | 0.21 | 0.21 | 2.96 | 0.97 | 0.75 | 0.75 |
| 4 | 0.12 | 6.30 | 3.87 | 0.39 | 4.98 | 9.04 | 6.60 | 3.09 |
| 6 | 0.42 | 10.81 | 6.51 | 0.26 | 6.75 | 15.84 | 11.53 | 5.23 |

Shared counters, hash map, NKEYS 1: map cost | total (us)
| Threads | kernel map | c98951a4 map | cb3f29d2 map | kernel total | c98951a4 total | cb3f29d2 total |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.02 | 0.03 | 0.02 | 1.26 | 0.23 | 0.23 |
| 2 | 0.08 | 0.20 | 0.12 | 1.92 | 0.48 | 0.39 |
| 4 | -0.17 | 1.15 | 0.02 | 3.29 | 3.46 | 2.29 |
| 6 | 0.13 | 1.16 | 0.03 | 4.16 | 5.97 | 4.81 |

Shared counters, hash map, NKEYS 64: map cost | total (us)
| Threads | kernel map | c98951a4 map | cb3f29d2 map | kernel total | c98951a4 total | cb3f29d2 total |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.03 | 0.03 | 0.03 | 1.27 | 0.24 | 0.24 |
| 2 | 0.06 | 0.20 | 0.12 | 1.91 | 0.48 | 0.40 |
| 4 | 0.13 | 0.69 | 0.07 | 3.58 | 3.01 | 2.36 |
| 6 | 0.05 | 1.07 | 0.02 | 4.11 | 5.88 | 4.79 |

Tracking table, 1,000 live entries (one op = lookup + delete + insert): map cost | total (us)
| Threads | kernel map | c98951a4 map | cb3f29d2 map | kernel total | c98951a4 total | cb3f29d2 total |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.49 | 0.49 | 0.57 | 3.01 | 0.98 | 1.06 |
| 2 | 0.48 | 0.78 | 0.62 | 4.14 | 1.53 | 1.35 |
| 4 | 0.36 | 5.30 | -1.60 | 6.72 | 9.52 | 2.59 |
| 6 | 0.15 | 10.10 | -0.90 | 7.68 | 16.16 | 5.18 |

Tracking table, 900,000 live entries (one op = lookup + delete + insert): map cost | total (us)
| Threads | kernel map | c98951a4 map | cb3f29d2 map | kernel total | c98951a4 total | cb3f29d2 total |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1.09 | 0.76 | 0.83 | 3.60 | 1.24 | 1.31 |
| 2 | 0.98 | 1.48 | 0.86 | 4.62 | 2.20 | 1.60 |
| 4 | 0.94 | 5.51 | -1.61 | 7.31 | 9.70 | 2.63 |
| 6 | 0.52 | 11.35 | -0.96 | 7.95 | 17.35 | 5.06 |

runs: churn 96 (0 failed checks), counters 144 (0), tracking 144 (0)

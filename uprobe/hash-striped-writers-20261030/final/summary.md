
Churn (insert at entry, lookup + delete at return), 4 B key, 8 B value: map cost | total (us)
| Threads | kernel map | c98951a4 map | cb3f29d2 map | f4fdcbcd map | kernel total | c98951a4 total | cb3f29d2 total | f4fdcbcd total |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.23 | 0.12 | 0.22 | 0.16 | 2.05 | 0.57 | 0.66 | 0.61 |
| 2 | 0.32 | 0.42 | 0.24 | 0.16 | 3.02 | 0.96 | 0.78 | 0.70 |
| 4 | 0.21 | 6.24 | 0.35 | 0.44 | 5.01 | 8.97 | 3.08 | 3.20 |
| 6 | 0.25 | 10.41 | 0.22 | 0.09 | 6.68 | 15.43 | 5.19 | 5.07 |

Shared counters, hash map, NKEYS 1: map cost | total (us)
| Threads | kernel map | c98951a4 map | cb3f29d2 map | f4fdcbcd map | kernel total | c98951a4 total | cb3f29d2 total | f4fdcbcd total |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.01 | 0.03 | 0.02 | 0.02 | 1.26 | 0.23 | 0.23 | 0.23 |
| 2 | 0.00 | 0.20 | 0.11 | 0.11 | 1.85 | 0.48 | 0.39 | 0.39 |
| 4 | -0.14 | 0.98 | 0.04 | -0.01 | 3.29 | 3.31 | 2.31 | 2.26 |
| 6 | 0.01 | 0.83 | 0.07 | 0.04 | 4.10 | 5.61 | 4.81 | 4.79 |

Shared counters, hash map, NKEYS 64: map cost | total (us)
| Threads | kernel map | c98951a4 map | cb3f29d2 map | f4fdcbcd map | kernel total | c98951a4 total | cb3f29d2 total | f4fdcbcd total |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.02 | 0.03 | 0.03 | 0.03 | 1.27 | 0.24 | 0.24 | 0.24 |
| 2 | 0.09 | 0.20 | 0.12 | 0.11 | 1.91 | 0.48 | 0.40 | 0.39 |
| 4 | 0.08 | 0.68 | 0.07 | 0.03 | 3.43 | 2.97 | 2.37 | 2.32 |
| 6 | 0.07 | 0.94 | 0.07 | 0.06 | 4.18 | 5.68 | 4.78 | 4.81 |

Tracking table, 1,000 live entries (one op = lookup + delete + insert): map cost | total (us)
| Threads | kernel map | c98951a4 map | cb3f29d2 map | f4fdcbcd map | kernel total | c98951a4 total | cb3f29d2 total | f4fdcbcd total |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.47 | 0.49 | 0.57 | 0.54 | 3.02 | 0.98 | 1.06 | 1.03 |
| 2 | 0.33 | 0.79 | 0.61 | 0.54 | 4.06 | 1.52 | 1.35 | 1.28 |
| 4 | 0.67 | 5.25 | -1.65 | -1.69 | 7.06 | 9.50 | 2.59 | 2.50 |
| 6 | 0.06 | 10.66 | -0.82 | -0.73 | 7.49 | 16.72 | 5.21 | 5.26 |

Tracking table, 900,000 live entries (one op = lookup + delete + insert): map cost | total (us)
| Threads | kernel map | c98951a4 map | cb3f29d2 map | f4fdcbcd map | kernel total | c98951a4 total | cb3f29d2 total | f4fdcbcd total |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1.08 | 0.75 | 0.83 | 0.75 | 3.61 | 1.24 | 1.31 | 1.24 |
| 2 | 0.95 | 1.48 | 0.87 | 0.81 | 4.68 | 2.20 | 1.60 | 1.55 |
| 4 | 0.79 | 5.51 | -1.60 | -1.73 | 7.34 | 9.70 | 2.64 | 2.46 |
| 6 | 0.95 | 11.36 | -0.96 | -0.89 | 8.14 | 17.41 | 5.06 | 5.16 |

runs: churn 96 (0 failed checks), counters 192 (0), tracking 192 (0)


Churn, 4 B key, 8 B value: total | map cost (us per call)
| Threads | dev-6ab9cbfe total | dev-5b28dae5 total | change | dev-6ab9cbfe map | dev-5b28dae5 map |
|---:|---:|---:|---:|---:|---:|
| 1 | 0.622 | 0.572 | -7.9% | 0.166 | 0.122 |
| 6 | 5.082 | 5.061 | -0.4% | 0.024 | 0.047 |

Tracking table, 1000 live: total | map cost (us per call)
| Threads | dev-6ab9cbfe total | dev-5b28dae5 total | change | dev-6ab9cbfe map | dev-5b28dae5 map |
|---:|---:|---:|---:|---:|---:|
| 1 | 1.032 | 1.015 | -1.6% | 0.542 | 0.525 |
| 6 | 5.159 | 5.158 | -0.0% | -0.788 | -0.835 |

Tracking table, 900000 live: total | map cost (us per call)
| Threads | dev-6ab9cbfe total | dev-5b28dae5 total | change | dev-6ab9cbfe map | dev-5b28dae5 map |
|---:|---:|---:|---:|---:|---:|
| 1 | 1.256 | 1.231 | -2.0% | 0.769 | 0.745 |
| 6 | 5.165 | 5.061 | -2.0% | -0.852 | -0.936 |

Shared counters, hash, NKEYS 1: total | map cost (us per call)
| Threads | dev-6ab9cbfe total | dev-5b28dae5 total | change | dev-6ab9cbfe map | dev-5b28dae5 map |
|---:|---:|---:|---:|---:|---:|
| 1 | 0.241 | 0.234 | -2.9% | 0.027 | 0.024 |
| 6 | 4.848 | 4.667 | -3.7% | -0.021 | -0.013 |

Shared counters, hash, NKEYS 64: total | map cost (us per call)
| Threads | dev-6ab9cbfe total | dev-5b28dae5 total | change | dev-6ab9cbfe map | dev-5b28dae5 map |
|---:|---:|---:|---:|---:|---:|
| 1 | 0.248 | 0.239 | -3.6% | 0.035 | 0.029 |
| 6 | 4.820 | 4.689 | -2.7% | -0.043 | 0.009 |

Shared counters, percpu-hash, NKEYS 1: total | map cost (us per call)
| Threads | dev-6ab9cbfe total | dev-5b28dae5 total | change | dev-6ab9cbfe map | dev-5b28dae5 map |
|---:|---:|---:|---:|---:|---:|
| 1 | 0.260 | 0.261 | +0.3% | 0.046 | 0.050 |
| 6 | 5.062 | 4.925 | -2.7% | 0.194 | 0.245 |

Shared counters, percpu-hash, NKEYS 64: total | map cost (us per call)
| Threads | dev-6ab9cbfe total | dev-5b28dae5 total | change | dev-6ab9cbfe map | dev-5b28dae5 map |
|---:|---:|---:|---:|---:|---:|
| 1 | 0.262 | 0.262 | +0.1% | 0.049 | 0.053 |
| 6 | 5.124 | 4.939 | -3.6% | 0.262 | 0.259 |

runs: churn 24 (0 not ok), track 48 (0), count 72 (0)

Official suite, userspace leg, ns per map operation (medians of 3)

| Case | a7d54d7e | dev-6ab9cbfe | dev-5b28dae5 | 5b28dae5 vs 6ab9cbfe | 5b28dae5 vs a7d54d7e |
|---|---:|---:|---:|---:|---:|
| uprobe_uretprobe | 0.33 | 0.33 | 0.33 | -0.4% | -0.2% |
| uretprobe | 0.27 | 0.27 | 0.27 | +0.1% | -0.5% |
| uprobe | 0.20 | 0.20 | 0.20 | -1.4% | -1.7% |
| read | 4.57 | 4.57 | 4.58 | +0.2% | +0.2% |
| write | 4.28 | 4.28 | 4.28 | +0.0% | +0.0% |
| hash_map_update | 28.26 | 42.89 | 29.50 | -31.2% | +4.4% |
| hash_map_lookup | 23.54 | 27.54 | 22.26 | -19.2% | -5.4% |
| hash_map_delete | 28.51 | 43.76 | 37.50 | -14.3% | +31.5% |
| array_map_update | 11.53 | 11.52 | 11.56 | +0.4% | +0.2% |
| array_map_lookup | 1.17 | 1.18 | 1.18 | -0.1% | +0.1% |
| array_map_delete | 21.27 | 20.91 | 20.51 | -1.9% | -3.6% |
| per_cpu_hash_map_update | 44.42 | 58.03 | 58.04 | +0.0% | +30.7% |
| per_cpu_hash_map_lookup | 27.74 | 34.85 | 34.49 | -1.0% | +24.3% |
| per_cpu_hash_map_delete | 99.62 | 105.87 | 105.99 | +0.1% | +6.4% |
| per_cpu_array_map_update | 14.46 | 14.80 | 14.80 | -0.0% | +2.4% |
| per_cpu_array_map_lookup | 4.11 | 4.10 | 4.11 | +0.2% | +0.0% |
| per_cpu_array_map_delete | 21.31 | 20.96 | 20.57 | -1.9% | -3.5% |

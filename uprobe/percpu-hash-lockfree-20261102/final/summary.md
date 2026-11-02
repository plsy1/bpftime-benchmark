
Shared counters, percpu-hash, NKEYS 1: total | map cost (us per call per thread)
| Threads | kernel total | dev-68f19c1f total | dev-a4ac0d31 total | change | kernel map | dev-68f19c1f map | dev-a4ac0d31 map |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1.268 | 0.260 | 0.244 | -6.2% | 0.026 | 0.049 | 0.032 |
| 2 | 1.841 | 0.460 | 0.342 | -25.7% | 0.024 | 0.184 | 0.062 |
| 4 | 3.365 | 2.853 | 2.302 | -19.3% | -0.002 | 0.433 | 0.009 |
| 6 | 4.221 | 4.982 | 4.714 | -5.4% | 0.117 | 0.300 | -0.040 |

Shared counters, percpu-hash, NKEYS 64: total | map cost (us per call per thread)
| Threads | kernel total | dev-68f19c1f total | dev-a4ac0d31 total | change | kernel map | dev-68f19c1f map | dev-a4ac0d31 map |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1.268 | 0.262 | 0.247 | -5.7% | 0.018 | 0.051 | 0.034 |
| 2 | 1.863 | 0.454 | 0.349 | -23.1% | 0.046 | 0.179 | 0.070 |
| 4 | 3.456 | 2.559 | 2.319 | -9.4% | -0.072 | 0.147 | 0.046 |
| 6 | 4.084 | 4.918 | 4.733 | -3.8% | 0.040 | 0.185 | -0.011 |

Shared counters, hash, NKEYS 1: total | map cost (us per call per thread)
| Threads | kernel total | dev-68f19c1f total | dev-a4ac0d31 total | change | kernel map | dev-68f19c1f map | dev-a4ac0d31 map |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1.262 | 0.234 | 0.235 | +0.5% | 0.021 | 0.023 | 0.022 |
| 2 | 1.861 | 0.389 | 0.394 | +1.3% | 0.044 | 0.114 | 0.115 |
| 4 | 3.280 | 2.224 | 2.261 | +1.6% | -0.087 | -0.196 | -0.032 |
| 6 | 4.094 | 4.704 | 4.739 | +0.8% | -0.010 | 0.022 | -0.015 |

Shared counters, hash, NKEYS 64: total | map cost (us per call per thread)
| Threads | kernel total | dev-68f19c1f total | dev-a4ac0d31 total | change | kernel map | dev-68f19c1f map | dev-a4ac0d31 map |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1.261 | 0.240 | 0.241 | +0.4% | 0.012 | 0.029 | 0.028 |
| 2 | 1.847 | 0.395 | 0.398 | +0.8% | 0.029 | 0.119 | 0.118 |
| 4 | 3.584 | 2.252 | 2.308 | +2.5% | 0.056 | -0.161 | 0.035 |
| 6 | 4.122 | 4.761 | 4.776 | +0.3% | 0.077 | 0.029 | 0.032 |

Churn (hash), 4 B key, 8 B value: total | map cost (us per call per thread)
| Threads | dev-68f19c1f total | dev-a4ac0d31 total | change | dev-68f19c1f map | dev-a4ac0d31 map |
|---:|---:|---:|---:|---:|---:|
| 1 | 0.590 | 0.576 | -2.4% | 0.135 | 0.122 |
| 6 | 5.059 | 5.023 | -0.7% | 0.075 | 0.055 |

Tracking table (hash), 1000 live: total | map cost (us per call per thread)
| Threads | dev-68f19c1f total | dev-a4ac0d31 total | change | dev-68f19c1f map | dev-a4ac0d31 map |
|---:|---:|---:|---:|---:|---:|
| 1 | 1.014 | 1.011 | -0.3% | 0.525 | 0.518 |
| 6 | 5.234 | 5.244 | +0.2% | -0.768 | -0.700 |

Tracking table (hash), 900000 live: total | map cost (us per call per thread)
| Threads | dev-68f19c1f total | dev-a4ac0d31 total | change | dev-68f19c1f map | dev-a4ac0d31 map |
|---:|---:|---:|---:|---:|---:|
| 1 | 1.236 | 1.229 | -0.6% | 0.751 | 0.740 |
| 6 | 5.047 | 5.050 | +0.1% | -0.955 | -0.901 |

runs: count 216 (0 not ok), churn 24 (0), track 48 (0); "change" = dev-a4ac0d31 vs dev-68f19c1f

Official suite, userspace leg, ns per map operation (medians of 5)

| Case | a7d54d7e | dev-68f19c1f | dev-a4ac0d31 | a4ac0d31 vs 68f19c1f | a4ac0d31 vs a7d54d7e |
|---|---:|---:|---:|---:|---:|
| uprobe_uretprobe | 0.33 | 0.33 | 0.33 | -0.1% | -0.2% |
| uretprobe | 0.27 | 0.27 | 0.27 | -0.9% | -0.7% |
| uprobe | 0.20 | 0.20 | 0.20 | +1.4% | +1.0% |
| read | 4.57 | 4.57 | 4.85 | +6.3% | +6.3% |
| write | 4.28 | 4.27 | 4.71 | +10.2% | +10.1% |
| hash_map_update | 27.12 | 29.49 | 29.11 | -1.3% | +7.3% |
| hash_map_lookup | 23.02 | 22.34 | 22.27 | -0.3% | -3.2% |
| hash_map_delete | 27.48 | 37.53 | 33.91 | -9.6% | +23.4% |
| array_map_update | 11.50 | 11.53 | 11.55 | +0.2% | +0.4% |
| array_map_lookup | 1.17 | 1.18 | 1.18 | +0.2% | +0.3% |
| array_map_delete | 21.25 | 20.95 | 21.02 | +0.3% | -1.1% |
| per_cpu_hash_map_update | 44.51 | 54.61 | 36.33 | -33.5% | -18.4% |
| per_cpu_hash_map_lookup | 27.72 | 32.82 | 25.17 | -23.3% | -9.2% |
| per_cpu_hash_map_delete | 99.58 | 104.96 | 58.22 | -44.5% | -41.5% |
| per_cpu_array_map_update | 14.43 | 14.86 | 14.91 | +0.3% | +3.4% |
| per_cpu_array_map_lookup | 4.11 | 4.11 | 4.11 | +0.1% | +0.1% |
| per_cpu_array_map_delete | 21.30 | 21.03 | 21.28 | +1.2% | -0.1% |

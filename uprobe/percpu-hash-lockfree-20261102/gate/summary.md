
Shared counters, percpu-hash, NKEYS 1: total | map cost (us per call per thread)
| Threads | kernel total | dev-68f19c1f total | dev-e1677f0d total | change | kernel map | dev-68f19c1f map | dev-e1677f0d map |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1.270 | 0.260 | 0.244 | -6.1% | 0.024 | 0.049 | 0.033 |
| 2 | 1.899 | 0.457 | 0.346 | -24.3% | 0.048 | 0.182 | 0.071 |
| 4 | 3.300 | 2.826 | 2.261 | -20.0% | -0.032 | 0.476 | 0.012 |
| 6 | 4.155 | 4.888 | 4.688 | -4.1% | 0.148 | 0.187 | 0.015 |

Shared counters, percpu-hash, NKEYS 64: total | map cost (us per call per thread)
| Threads | kernel total | dev-68f19c1f total | dev-e1677f0d total | change | kernel map | dev-68f19c1f map | dev-e1677f0d map |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1.268 | 0.261 | 0.247 | -5.4% | 0.022 | 0.050 | 0.036 |
| 2 | 1.893 | 0.454 | 0.347 | -23.6% | 0.048 | 0.179 | 0.070 |
| 4 | 3.323 | 2.535 | 2.288 | -9.7% | -0.165 | 0.200 | 0.029 |
| 6 | 4.136 | 4.909 | 4.758 | -3.1% | 0.033 | 0.221 | 0.071 |

Shared counters, hash, NKEYS 1: total | map cost (us per call per thread)
| Threads | kernel total | dev-68f19c1f total | dev-e1677f0d total | change | kernel map | dev-68f19c1f map | dev-e1677f0d map |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1.262 | 0.234 | 0.235 | +0.7% | 0.016 | 0.023 | 0.024 |
| 2 | 1.920 | 0.390 | 0.391 | +0.4% | 0.069 | 0.115 | 0.117 |
| 4 | 3.529 | 2.262 | 2.242 | -0.9% | 0.196 | -0.087 | -0.007 |
| 6 | 4.088 | 4.694 | 4.772 | +1.7% | 0.081 | -0.007 | 0.098 |

Shared counters, hash, NKEYS 64: total | map cost (us per call per thread)
| Threads | kernel total | dev-68f19c1f total | dev-e1677f0d total | change | kernel map | dev-68f19c1f map | dev-e1677f0d map |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1.261 | 0.240 | 0.241 | +0.5% | 0.015 | 0.029 | 0.030 |
| 2 | 1.910 | 0.394 | 0.395 | +0.2% | 0.065 | 0.119 | 0.119 |
| 4 | 3.526 | 2.261 | 2.307 | +2.0% | 0.037 | -0.074 | 0.048 |
| 6 | 4.183 | 4.762 | 4.790 | +0.6% | 0.081 | 0.074 | 0.102 |

Churn (hash), 4 B key, 8 B value: total | map cost (us per call per thread)
| Threads | dev-68f19c1f total | dev-e1677f0d total | change | dev-68f19c1f map | dev-e1677f0d map |
|---:|---:|---:|---:|---:|---:|
| 1 | 0.590 | 0.595 | +0.8% | 0.137 | 0.142 |
| 6 | 5.000 | 5.033 | +0.7% | -0.012 | 0.021 |

Tracking table (hash), 1000 live: total | map cost (us per call per thread)
| Threads | dev-68f19c1f total | dev-e1677f0d total | change | dev-68f19c1f map | dev-e1677f0d map |
|---:|---:|---:|---:|---:|---:|
| 1 | 1.015 | 1.019 | +0.4% | 0.524 | 0.528 |
| 6 | 5.223 | 5.224 | +0.0% | -0.854 | -0.762 |

Tracking table (hash), 900000 live: total | map cost (us per call per thread)
| Threads | dev-68f19c1f total | dev-e1677f0d total | change | dev-68f19c1f map | dev-e1677f0d map |
|---:|---:|---:|---:|---:|---:|
| 1 | 1.237 | 1.236 | -0.1% | 0.753 | 0.750 |
| 6 | 5.063 | 5.059 | -0.1% | -0.989 | -0.970 |

runs: count 216 (0 not ok), churn 24 (0), track 48 (0); "change" = dev-e1677f0d vs dev-68f19c1f

Official suite, userspace leg, ns per map operation (medians of 3)

| Case | a7d54d7e | dev-68f19c1f | dev-e1677f0d | e1677f0d vs 68f19c1f | e1677f0d vs a7d54d7e |
|---|---:|---:|---:|---:|---:|
| uprobe_uretprobe | 0.33 | 0.33 | 0.33 | +0.1% | -0.1% |
| uretprobe | 0.27 | 0.27 | 0.28 | +2.6% | +2.3% |
| uprobe | 0.20 | 0.20 | 0.20 | +0.1% | -0.5% |
| read | 4.57 | 4.57 | 4.85 | +6.3% | +6.3% |
| write | 4.27 | 4.27 | 4.85 | +13.4% | +13.5% |
| hash_map_update | 27.14 | 29.52 | 30.07 | +1.9% | +10.8% |
| hash_map_lookup | 23.01 | 22.35 | 22.23 | -0.5% | -3.4% |
| hash_map_delete | 27.54 | 37.52 | 39.12 | +4.3% | +42.1% |
| array_map_update | 11.58 | 11.54 | 11.53 | -0.1% | -0.4% |
| array_map_lookup | 1.18 | 1.18 | 1.18 | +0.1% | +0.0% |
| array_map_delete | 21.41 | 20.91 | 20.63 | -1.3% | -3.6% |
| per_cpu_hash_map_update | 44.48 | 54.74 | 34.74 | -36.5% | -21.9% |
| per_cpu_hash_map_lookup | 27.77 | 33.47 | 25.31 | -24.4% | -8.9% |
| per_cpu_hash_map_delete | 99.61 | 104.09 | 62.68 | -39.8% | -37.1% |
| per_cpu_array_map_update | 14.47 | 14.86 | 14.90 | +0.3% | +3.0% |
| per_cpu_array_map_lookup | 4.11 | 4.10 | 4.11 | +0.2% | +0.1% |
| per_cpu_array_map_delete | 21.38 | 21.01 | 20.69 | -1.5% | -3.2% |

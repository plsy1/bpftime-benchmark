# BPFtime Uprobe Benchmark Results

*Generated on 2026-11-01 05:07:20*

## Environment

- **OS:** Linux 6.8.12-1021-tegra-btf
- **CPU:** Unknown (0 cores, 6 threads)
- **Memory:** 7.36 GB
- **Python:** 3.12.3

## Summary

This benchmark compares three different eBPF execution environments:
- **Kernel Uprobe**: Traditional kernel-based eBPF uprobes
- **Userspace Uprobe**: BPFtime's userspace implementation of uprobes
- **Embedded VM**: BPFtime's embedded eBPF VM

*Times shown in nanoseconds (ns) - lower is better*

### Core Uprobe Performance Summary

| Operation | Kernel Uprobe | Userspace Uprobe | Speedup |
|-----------|---------------|------------------|---------|
| __bench_uprobe | 1240.73 | 201.40 | 6.16x |
| __bench_uretprobe | 1659.05 | 269.86 | 6.15x |
| __bench_uprobe_uretprobe | 1783.03 | 327.84 | 5.44x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3568.46 | 3576.54 | 3571.67 | 2.54 |
| __bench_array_map_delete | Userspace | 20891.54 | 21032.34 | 20963.97 | 53.95 |
| __bench_array_map_lookup | Kernel | 3794.25 | 3808.77 | 3801.51 | 5.22 |
| __bench_array_map_lookup | Userspace | 1172.11 | 1180.55 | 1176.79 | 2.19 |
| __bench_array_map_update | Kernel | 13078.05 | 13124.70 | 13093.41 | 13.18 |
| __bench_array_map_update | Userspace | 11511.40 | 11803.07 | 11590.90 | 103.03 |
| __bench_hash_map_delete | Kernel | 108136.45 | 108356.37 | 108262.27 | 62.38 |
| __bench_hash_map_delete | Userspace | 43626.75 | 44443.53 | 43986.34 | 337.86 |
| __bench_hash_map_lookup | Kernel | 28437.79 | 28698.76 | 28537.37 | 72.53 |
| __bench_hash_map_lookup | Userspace | 26997.64 | 27664.03 | 27508.00 | 176.30 |
| __bench_hash_map_update | Kernel | 95153.69 | 95389.97 | 95284.79 | 73.93 |
| __bench_hash_map_update | Userspace | 42777.43 | 43062.67 | 42922.26 | 112.77 |
| __bench_per_cpu_array_map_delete | Kernel | 3742.54 | 3753.99 | 3748.46 | 4.06 |
| __bench_per_cpu_array_map_delete | Userspace | 20979.71 | 21985.04 | 21162.18 | 280.97 |
| __bench_per_cpu_array_map_lookup | Kernel | 4516.44 | 4527.42 | 4520.55 | 3.34 |
| __bench_per_cpu_array_map_lookup | Userspace | 4098.08 | 4128.98 | 4110.87 | 10.58 |
| __bench_per_cpu_array_map_update | Kernel | 16588.66 | 16618.04 | 16606.79 | 8.74 |
| __bench_per_cpu_array_map_update | Userspace | 14785.97 | 15167.30 | 14910.27 | 114.28 |
| __bench_per_cpu_hash_map_delete | Kernel | 150968.09 | 151145.26 | 151061.08 | 51.14 |
| __bench_per_cpu_hash_map_delete | Userspace | 105337.30 | 107687.16 | 105843.81 | 645.96 |
| __bench_per_cpu_hash_map_lookup | Kernel | 30323.24 | 30440.19 | 30379.40 | 42.11 |
| __bench_per_cpu_hash_map_lookup | Userspace | 34677.20 | 35095.33 | 34968.37 | 136.89 |
| __bench_per_cpu_hash_map_update | Kernel | 68194.65 | 68311.10 | 68241.95 | 36.54 |
| __bench_per_cpu_hash_map_update | Userspace | 57726.74 | 58319.70 | 58051.76 | 204.20 |
| __bench_read | Kernel | 17516.43 | 21271.49 | 18271.75 | 1337.03 |
| __bench_read | Userspace | 4569.11 | 4577.65 | 4574.37 | 2.16 |
| __bench_uprobe | Kernel | 1237.15 | 1244.18 | 1240.73 | 2.28 |
| __bench_uprobe | Userspace | 199.83 | 202.78 | 201.40 | 0.90 |
| __bench_uprobe_uretprobe | Kernel | 1770.41 | 1804.57 | 1783.03 | 10.67 |
| __bench_uprobe_uretprobe | Userspace | 326.32 | 330.68 | 327.84 | 1.21 |
| __bench_uretprobe | Kernel | 1655.18 | 1663.84 | 1659.05 | 2.66 |
| __bench_uretprobe | Userspace | 269.11 | 270.92 | 269.86 | 0.64 |
| __bench_write | Kernel | 17752.18 | 17901.25 | 17829.07 | 39.07 |
| __bench_write | Userspace | 4274.12 | 4282.68 | 4277.05 | 2.94 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 153.40 | 183.19 | 159.26 | 8.51 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-11-01 05:07:20
- **Total duration:** 1504.55 seconds

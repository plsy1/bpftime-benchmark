# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-17 23:32:52*

## Environment

- **OS:** Linux 6.8.12-1021-tegra
- **CPU:** Unknown (0 cores, 6 threads)
- **Memory:** 7.37 GB
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
| __bench_uprobe | 1226.55 | 198.29 | 6.19x |
| __bench_uretprobe | 1640.94 | 269.43 | 6.09x |
| __bench_uprobe_uretprobe | 1754.31 | 319.98 | 5.48x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3682.61 | 3690.61 | 3686.89 | 2.94 |
| __bench_array_map_delete | Userspace | 20034.12 | 20256.68 | 20138.20 | 76.92 |
| __bench_array_map_lookup | Kernel | 3804.86 | 3813.10 | 3806.89 | 2.22 |
| __bench_array_map_lookup | Userspace | 7919.60 | 7943.48 | 7929.17 | 8.20 |
| __bench_array_map_update | Kernel | 13076.60 | 13112.62 | 13095.97 | 12.10 |
| __bench_array_map_update | Userspace | 11519.46 | 11790.81 | 11571.44 | 75.07 |
| __bench_hash_map_delete | Kernel | 107858.66 | 107938.36 | 107896.69 | 22.49 |
| __bench_hash_map_delete | Userspace | 36568.22 | 38439.73 | 37039.79 | 505.06 |
| __bench_hash_map_lookup | Kernel | 28576.89 | 28881.41 | 28722.15 | 94.61 |
| __bench_hash_map_lookup | Userspace | 29471.23 | 29837.37 | 29572.76 | 115.24 |
| __bench_hash_map_update | Kernel | 94294.39 | 94370.15 | 94331.30 | 24.28 |
| __bench_hash_map_update | Userspace | 34559.32 | 38025.25 | 35979.65 | 1496.99 |
| __bench_per_cpu_array_map_delete | Kernel | 3514.61 | 3526.19 | 3517.24 | 3.32 |
| __bench_per_cpu_array_map_delete | Userspace | 20067.57 | 20264.84 | 20165.48 | 71.98 |
| __bench_per_cpu_array_map_lookup | Kernel | 4210.15 | 4232.04 | 4218.10 | 5.45 |
| __bench_per_cpu_array_map_lookup | Userspace | 11583.16 | 11690.58 | 11631.50 | 43.33 |
| __bench_per_cpu_array_map_update | Kernel | 16559.23 | 16616.54 | 16593.59 | 15.38 |
| __bench_per_cpu_array_map_update | Userspace | 14194.08 | 14597.85 | 14346.37 | 117.14 |
| __bench_per_cpu_hash_map_delete | Kernel | 152935.83 | 153311.22 | 153142.63 | 123.76 |
| __bench_per_cpu_hash_map_delete | Userspace | 98756.96 | 99502.94 | 98999.49 | 263.02 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29647.79 | 29952.50 | 29750.50 | 90.07 |
| __bench_per_cpu_hash_map_lookup | Userspace | 41497.04 | 41749.14 | 41621.84 | 90.57 |
| __bench_per_cpu_hash_map_update | Kernel | 68033.33 | 68336.83 | 68143.94 | 84.57 |
| __bench_per_cpu_hash_map_update | Userspace | 44264.50 | 44496.60 | 44395.95 | 74.94 |
| __bench_read | Kernel | 17624.28 | 17697.27 | 17656.67 | 19.77 |
| __bench_read | Userspace | 4578.53 | 4594.29 | 4582.75 | 5.30 |
| __bench_uprobe | Kernel | 1224.43 | 1232.54 | 1226.55 | 2.55 |
| __bench_uprobe | Userspace | 194.90 | 202.50 | 198.29 | 2.54 |
| __bench_uprobe_uretprobe | Kernel | 1749.29 | 1764.62 | 1754.31 | 4.55 |
| __bench_uprobe_uretprobe | Userspace | 319.09 | 321.72 | 319.98 | 0.73 |
| __bench_uretprobe | Kernel | 1636.44 | 1650.92 | 1640.94 | 4.30 |
| __bench_uretprobe | Userspace | 268.69 | 271.11 | 269.43 | 0.76 |
| __bench_write | Kernel | 17660.72 | 17740.21 | 17706.48 | 22.12 |
| __bench_write | Userspace | 4270.92 | 4274.04 | 4271.94 | 1.01 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 155.64 | 164.54 | 158.29 | 2.70 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-17 23:32:52
- **Total duration:** 1516.54 seconds

# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-21 21:11:00*

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
| __bench_uprobe | 1229.29 | 198.89 | 6.18x |
| __bench_uretprobe | 1641.62 | 270.29 | 6.07x |
| __bench_uprobe_uretprobe | 1756.76 | 323.76 | 5.43x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3683.28 | 3698.71 | 3688.16 | 5.01 |
| __bench_array_map_delete | Userspace | 20086.25 | 20296.40 | 20163.59 | 71.25 |
| __bench_array_map_lookup | Kernel | 3806.80 | 3818.09 | 3810.64 | 4.38 |
| __bench_array_map_lookup | Userspace | 1168.80 | 1181.76 | 1172.55 | 3.53 |
| __bench_array_map_update | Kernel | 13071.69 | 13106.84 | 13085.29 | 12.22 |
| __bench_array_map_update | Userspace | 11527.28 | 11601.35 | 11565.74 | 24.12 |
| __bench_hash_map_delete | Kernel | 107880.91 | 107991.44 | 107948.44 | 31.11 |
| __bench_hash_map_delete | Userspace | 36235.06 | 36446.24 | 36318.01 | 66.03 |
| __bench_hash_map_lookup | Kernel | 28493.12 | 28850.11 | 28607.97 | 94.88 |
| __bench_hash_map_lookup | Userspace | 26844.69 | 27339.38 | 27097.94 | 127.88 |
| __bench_hash_map_update | Kernel | 93138.45 | 93237.58 | 93176.59 | 25.35 |
| __bench_hash_map_update | Userspace | 33016.32 | 36064.85 | 34112.94 | 1262.11 |
| __bench_per_cpu_array_map_delete | Kernel | 3672.90 | 3697.46 | 3686.65 | 6.76 |
| __bench_per_cpu_array_map_delete | Userspace | 20186.57 | 20390.51 | 20292.67 | 88.92 |
| __bench_per_cpu_array_map_lookup | Kernel | 4403.51 | 4434.10 | 4414.81 | 8.65 |
| __bench_per_cpu_array_map_lookup | Userspace | 4111.64 | 4131.39 | 4119.90 | 6.81 |
| __bench_per_cpu_array_map_update | Kernel | 16612.68 | 16693.73 | 16631.70 | 23.14 |
| __bench_per_cpu_array_map_update | Userspace | 14292.83 | 14415.21 | 14365.94 | 37.13 |
| __bench_per_cpu_hash_map_delete | Kernel | 151827.16 | 151994.38 | 151905.09 | 51.41 |
| __bench_per_cpu_hash_map_delete | Userspace | 98329.68 | 99341.08 | 98716.98 | 294.36 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29297.09 | 29457.43 | 29388.03 | 53.37 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27641.12 | 27834.87 | 27733.13 | 67.58 |
| __bench_per_cpu_hash_map_update | Kernel | 68192.90 | 68238.39 | 68219.31 | 12.60 |
| __bench_per_cpu_hash_map_update | Userspace | 44555.33 | 44686.94 | 44619.54 | 49.26 |
| __bench_read | Kernel | 18031.10 | 18283.71 | 18136.46 | 71.07 |
| __bench_read | Userspace | 4572.98 | 4596.20 | 4584.51 | 9.23 |
| __bench_uprobe | Kernel | 1226.15 | 1234.14 | 1229.29 | 2.56 |
| __bench_uprobe | Userspace | 197.77 | 200.45 | 198.89 | 0.79 |
| __bench_uprobe_uretprobe | Kernel | 1750.04 | 1778.80 | 1756.76 | 8.08 |
| __bench_uprobe_uretprobe | Userspace | 321.99 | 324.68 | 323.76 | 0.87 |
| __bench_uretprobe | Kernel | 1637.33 | 1647.33 | 1641.62 | 3.40 |
| __bench_uretprobe | Userspace | 268.31 | 272.12 | 270.29 | 1.25 |
| __bench_write | Kernel | 17660.06 | 17740.86 | 17698.50 | 28.75 |
| __bench_write | Userspace | 4271.29 | 4276.22 | 4273.78 | 1.70 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 154.25 | 173.12 | 159.49 | 5.07 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-21 21:11:00
- **Total duration:** 1484.50 seconds

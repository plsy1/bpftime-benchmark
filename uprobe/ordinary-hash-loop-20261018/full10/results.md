# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-18 15:25:45*

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
| __bench_uprobe | 1228.00 | 193.50 | 6.35x |
| __bench_uretprobe | 1660.61 | 268.60 | 6.18x |
| __bench_uprobe_uretprobe | 1771.68 | 320.20 | 5.53x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3691.32 | 3700.02 | 3694.83 | 3.73 |
| __bench_array_map_delete | Userspace | 20023.32 | 21271.84 | 20187.52 | 363.06 |
| __bench_array_map_lookup | Kernel | 3805.19 | 3814.28 | 3809.03 | 3.79 |
| __bench_array_map_lookup | Userspace | 7929.68 | 8651.53 | 8007.05 | 214.89 |
| __bench_array_map_update | Kernel | 13194.48 | 13220.11 | 13209.88 | 8.42 |
| __bench_array_map_update | Userspace | 11557.27 | 12444.00 | 11668.84 | 258.71 |
| __bench_hash_map_delete | Kernel | 108721.33 | 108809.22 | 108755.24 | 30.25 |
| __bench_hash_map_delete | Userspace | 36460.49 | 39389.84 | 36826.09 | 857.56 |
| __bench_hash_map_lookup | Kernel | 29026.84 | 29205.00 | 29124.46 | 58.17 |
| __bench_hash_map_lookup | Userspace | 26773.95 | 27038.74 | 26864.17 | 112.55 |
| __bench_hash_map_update | Kernel | 94494.06 | 94591.71 | 94545.83 | 29.84 |
| __bench_hash_map_update | Userspace | 33341.42 | 36217.42 | 34320.06 | 1226.49 |
| __bench_per_cpu_array_map_delete | Kernel | 3654.94 | 3689.10 | 3680.67 | 8.99 |
| __bench_per_cpu_array_map_delete | Userspace | 20054.46 | 21083.49 | 20210.08 | 294.36 |
| __bench_per_cpu_array_map_lookup | Kernel | 4403.59 | 4423.73 | 4407.92 | 5.70 |
| __bench_per_cpu_array_map_lookup | Userspace | 11634.95 | 12416.25 | 11771.61 | 234.11 |
| __bench_per_cpu_array_map_update | Kernel | 16581.76 | 16609.42 | 16594.29 | 8.58 |
| __bench_per_cpu_array_map_update | Userspace | 14284.47 | 15002.86 | 14452.99 | 212.60 |
| __bench_per_cpu_hash_map_delete | Kernel | 152637.79 | 152817.05 | 152718.16 | 62.79 |
| __bench_per_cpu_hash_map_delete | Userspace | 98454.74 | 98698.87 | 98547.65 | 92.90 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29227.57 | 29385.91 | 29301.66 | 54.90 |
| __bench_per_cpu_hash_map_lookup | Userspace | 34888.77 | 35042.70 | 34953.02 | 43.61 |
| __bench_per_cpu_hash_map_update | Kernel | 68561.48 | 68685.93 | 68599.78 | 34.00 |
| __bench_per_cpu_hash_map_update | Userspace | 44358.65 | 44471.45 | 44395.88 | 42.26 |
| __bench_read | Kernel | 18065.95 | 18278.70 | 18130.72 | 58.47 |
| __bench_read | Userspace | 4574.84 | 4582.79 | 4579.01 | 1.96 |
| __bench_uprobe | Kernel | 1222.99 | 1243.11 | 1228.00 | 6.11 |
| __bench_uprobe | Userspace | 191.98 | 197.55 | 193.50 | 1.62 |
| __bench_uprobe_uretprobe | Kernel | 1764.48 | 1780.29 | 1771.68 | 4.82 |
| __bench_uprobe_uretprobe | Userspace | 318.44 | 322.01 | 320.20 | 1.20 |
| __bench_uretprobe | Kernel | 1656.11 | 1670.16 | 1660.61 | 4.14 |
| __bench_uretprobe | Userspace | 266.52 | 272.99 | 268.60 | 1.97 |
| __bench_write | Kernel | 17736.49 | 18920.40 | 17895.37 | 342.44 |
| __bench_write | Userspace | 4267.99 | 4274.31 | 4270.16 | 1.79 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 155.57 | 176.30 | 158.95 | 5.86 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-18 15:25:45
- **Total duration:** 1508.82 seconds

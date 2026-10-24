# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-15 18:08:42*

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
| __bench_uprobe | 1227.79 | 196.30 | 6.25x |
| __bench_uretprobe | 1642.35 | 267.36 | 6.14x |
| __bench_uprobe_uretprobe | 1755.39 | 319.27 | 5.50x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3684.63 | 3703.29 | 3690.18 | 5.75 |
| __bench_array_map_delete | Userspace | 24033.76 | 24239.30 | 24143.82 | 74.50 |
| __bench_array_map_lookup | Kernel | 3788.25 | 3800.17 | 3793.70 | 3.94 |
| __bench_array_map_lookup | Userspace | 23977.51 | 24347.35 | 24168.79 | 146.62 |
| __bench_array_map_update | Kernel | 13023.00 | 13065.07 | 13046.41 | 12.64 |
| __bench_array_map_update | Userspace | 31482.83 | 36536.32 | 32139.65 | 1471.09 |
| __bench_hash_map_delete | Kernel | 106938.95 | 107132.28 | 107025.77 | 57.63 |
| __bench_hash_map_delete | Userspace | 60030.07 | 60905.70 | 60406.73 | 364.66 |
| __bench_hash_map_lookup | Kernel | 27651.47 | 27899.50 | 27772.20 | 69.24 |
| __bench_hash_map_lookup | Userspace | 61844.79 | 63654.79 | 62641.01 | 590.14 |
| __bench_hash_map_update | Kernel | 93832.53 | 94066.11 | 93949.16 | 73.69 |
| __bench_hash_map_update | Userspace | 69674.83 | 70320.80 | 69943.20 | 229.66 |
| __bench_per_cpu_array_map_delete | Kernel | 3516.98 | 3526.35 | 3522.19 | 3.28 |
| __bench_per_cpu_array_map_delete | Userspace | 24053.99 | 24228.80 | 24142.96 | 57.24 |
| __bench_per_cpu_array_map_lookup | Kernel | 4217.69 | 4229.25 | 4220.30 | 3.07 |
| __bench_per_cpu_array_map_lookup | Userspace | 15766.78 | 16076.65 | 15923.76 | 114.69 |
| __bench_per_cpu_array_map_update | Kernel | 16635.58 | 16692.36 | 16662.06 | 20.40 |
| __bench_per_cpu_array_map_update | Userspace | 35266.01 | 40353.15 | 35881.86 | 1492.22 |
| __bench_per_cpu_hash_map_delete | Kernel | 151729.98 | 151938.27 | 151849.17 | 69.62 |
| __bench_per_cpu_hash_map_delete | Userspace | 121444.23 | 122209.47 | 121884.08 | 216.69 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29399.97 | 29646.07 | 29532.80 | 84.83 |
| __bench_per_cpu_hash_map_lookup | Userspace | 70445.44 | 70782.49 | 70605.26 | 97.13 |
| __bench_per_cpu_hash_map_update | Kernel | 68122.59 | 68264.00 | 68196.74 | 51.22 |
| __bench_per_cpu_hash_map_update | Userspace | 81986.27 | 85472.81 | 82630.95 | 964.14 |
| __bench_read | Kernel | 17991.34 | 18170.34 | 18095.16 | 45.35 |
| __bench_read | Userspace | 4567.14 | 4581.16 | 4573.82 | 4.40 |
| __bench_uprobe | Kernel | 1224.12 | 1237.06 | 1227.79 | 4.11 |
| __bench_uprobe | Userspace | 194.61 | 197.84 | 196.30 | 1.15 |
| __bench_uprobe_uretprobe | Kernel | 1748.51 | 1762.19 | 1755.39 | 4.14 |
| __bench_uprobe_uretprobe | Userspace | 318.16 | 321.13 | 319.27 | 0.91 |
| __bench_uretprobe | Kernel | 1637.30 | 1654.01 | 1642.35 | 5.36 |
| __bench_uretprobe | Userspace | 266.13 | 269.54 | 267.36 | 1.00 |
| __bench_write | Kernel | 17609.81 | 17926.71 | 17731.53 | 74.31 |
| __bench_write | Userspace | 4271.40 | 4741.83 | 4345.57 | 142.63 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.48 | 182.19 | 156.75 | 8.54 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-15 18:08:42
- **Total duration:** 1938.45 seconds

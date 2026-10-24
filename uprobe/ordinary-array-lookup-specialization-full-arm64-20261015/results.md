# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-15 20:34:34*

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
| __bench_uprobe | 1228.11 | 195.65 | 6.28x |
| __bench_uretprobe | 1641.77 | 265.08 | 6.19x |
| __bench_uprobe_uretprobe | 1754.24 | 318.16 | 5.51x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3681.99 | 3689.72 | 3685.76 | 3.19 |
| __bench_array_map_delete | Userspace | 24627.34 | 25237.95 | 24801.57 | 165.48 |
| __bench_array_map_lookup | Kernel | 3787.79 | 3802.76 | 3793.27 | 4.80 |
| __bench_array_map_lookup | Userspace | 10701.26 | 10736.38 | 10715.45 | 11.97 |
| __bench_array_map_update | Kernel | 13187.47 | 13274.47 | 13208.80 | 22.77 |
| __bench_array_map_update | Userspace | 31807.96 | 32426.68 | 31956.22 | 220.51 |
| __bench_hash_map_delete | Kernel | 106595.30 | 106681.38 | 106622.84 | 26.82 |
| __bench_hash_map_delete | Userspace | 59899.75 | 60866.90 | 60445.05 | 398.79 |
| __bench_hash_map_lookup | Kernel | 28494.25 | 28784.26 | 28629.22 | 75.97 |
| __bench_hash_map_lookup | Userspace | 62966.91 | 64603.45 | 63802.77 | 711.09 |
| __bench_hash_map_update | Kernel | 94215.02 | 94358.85 | 94287.01 | 38.47 |
| __bench_hash_map_update | Userspace | 70023.11 | 71019.05 | 70389.48 | 329.55 |
| __bench_per_cpu_array_map_delete | Kernel | 3680.74 | 3698.18 | 3685.96 | 5.25 |
| __bench_per_cpu_array_map_delete | Userspace | 24641.22 | 24786.73 | 24724.87 | 47.14 |
| __bench_per_cpu_array_map_lookup | Kernel | 4398.09 | 4444.42 | 4406.88 | 12.96 |
| __bench_per_cpu_array_map_lookup | Userspace | 14277.98 | 14414.30 | 14352.98 | 48.07 |
| __bench_per_cpu_array_map_update | Kernel | 16554.18 | 16615.92 | 16581.63 | 20.57 |
| __bench_per_cpu_array_map_update | Userspace | 35509.52 | 35860.66 | 35685.67 | 106.93 |
| __bench_per_cpu_hash_map_delete | Kernel | 152258.31 | 152370.75 | 152311.46 | 35.39 |
| __bench_per_cpu_hash_map_delete | Userspace | 117185.98 | 117600.63 | 117380.45 | 156.03 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29315.99 | 29490.96 | 29384.12 | 49.70 |
| __bench_per_cpu_hash_map_lookup | Userspace | 72165.76 | 72583.18 | 72346.88 | 116.88 |
| __bench_per_cpu_hash_map_update | Kernel | 68404.71 | 68519.21 | 68449.42 | 28.77 |
| __bench_per_cpu_hash_map_update | Userspace | 79597.55 | 80547.38 | 79999.11 | 309.25 |
| __bench_read | Kernel | 18023.57 | 18130.53 | 18095.30 | 33.99 |
| __bench_read | Userspace | 4563.43 | 4570.92 | 4567.67 | 2.00 |
| __bench_uprobe | Kernel | 1224.31 | 1244.29 | 1228.11 | 5.90 |
| __bench_uprobe | Userspace | 193.84 | 197.22 | 195.65 | 1.26 |
| __bench_uprobe_uretprobe | Kernel | 1745.77 | 1769.07 | 1754.24 | 6.49 |
| __bench_uprobe_uretprobe | Userspace | 317.34 | 319.29 | 318.16 | 0.60 |
| __bench_uretprobe | Kernel | 1636.18 | 1657.86 | 1641.77 | 6.07 |
| __bench_uretprobe | Userspace | 263.74 | 266.72 | 265.08 | 0.75 |
| __bench_write | Kernel | 17672.30 | 17746.44 | 17719.40 | 19.10 |
| __bench_write | Userspace | 4269.69 | 4273.38 | 4271.48 | 1.15 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 154.09 | 177.56 | 159.04 | 6.53 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-15 20:34:34
- **Total duration:** 1920.65 seconds

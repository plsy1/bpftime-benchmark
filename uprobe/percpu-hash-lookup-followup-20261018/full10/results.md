# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-18 21:10:44*

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
| __bench_uprobe | 1226.97 | 194.26 | 6.32x |
| __bench_uretprobe | 1641.26 | 267.29 | 6.14x |
| __bench_uprobe_uretprobe | 1754.63 | 318.43 | 5.51x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3684.19 | 3693.52 | 3688.56 | 2.99 |
| __bench_array_map_delete | Userspace | 20043.27 | 20236.77 | 20120.40 | 58.09 |
| __bench_array_map_lookup | Kernel | 3769.63 | 3813.73 | 3804.24 | 11.99 |
| __bench_array_map_lookup | Userspace | 7917.54 | 7943.21 | 7926.16 | 8.98 |
| __bench_array_map_update | Kernel | 13187.92 | 13232.48 | 13209.41 | 12.13 |
| __bench_array_map_update | Userspace | 11522.44 | 11578.53 | 11550.11 | 22.03 |
| __bench_hash_map_delete | Kernel | 107729.60 | 107910.38 | 107856.97 | 48.66 |
| __bench_hash_map_delete | Userspace | 36315.85 | 36704.41 | 36520.58 | 129.25 |
| __bench_hash_map_lookup | Kernel | 28031.29 | 28333.59 | 28193.65 | 96.73 |
| __bench_hash_map_lookup | Userspace | 26778.74 | 27148.99 | 26974.31 | 138.62 |
| __bench_hash_map_update | Kernel | 95426.03 | 95599.25 | 95536.58 | 63.08 |
| __bench_hash_map_update | Userspace | 33019.91 | 36281.30 | 34915.88 | 1354.93 |
| __bench_per_cpu_array_map_delete | Kernel | 3683.71 | 3697.00 | 3687.81 | 4.00 |
| __bench_per_cpu_array_map_delete | Userspace | 20057.01 | 20278.03 | 20141.76 | 62.86 |
| __bench_per_cpu_array_map_lookup | Kernel | 4401.52 | 4415.10 | 4405.60 | 3.92 |
| __bench_per_cpu_array_map_lookup | Userspace | 11589.67 | 11702.20 | 11639.77 | 36.55 |
| __bench_per_cpu_array_map_update | Kernel | 16587.00 | 16644.45 | 16603.03 | 16.84 |
| __bench_per_cpu_array_map_update | Userspace | 14273.80 | 14370.64 | 14324.06 | 34.72 |
| __bench_per_cpu_hash_map_delete | Kernel | 151508.44 | 151635.27 | 151578.79 | 45.63 |
| __bench_per_cpu_hash_map_delete | Userspace | 98469.91 | 99604.81 | 98960.28 | 380.05 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29887.01 | 30010.99 | 29969.13 | 33.19 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27576.61 | 28870.11 | 27815.27 | 358.66 |
| __bench_per_cpu_hash_map_update | Kernel | 68152.84 | 68281.41 | 68229.48 | 38.88 |
| __bench_per_cpu_hash_map_update | Userspace | 44393.04 | 45902.01 | 44590.88 | 438.00 |
| __bench_read | Kernel | 18065.83 | 18219.44 | 18128.49 | 47.69 |
| __bench_read | Userspace | 4575.72 | 4582.57 | 4578.74 | 2.27 |
| __bench_uprobe | Kernel | 1224.15 | 1232.78 | 1226.97 | 2.45 |
| __bench_uprobe | Userspace | 192.82 | 196.15 | 194.26 | 1.05 |
| __bench_uprobe_uretprobe | Kernel | 1750.81 | 1758.78 | 1754.63 | 2.74 |
| __bench_uprobe_uretprobe | Userspace | 317.24 | 320.25 | 318.43 | 1.06 |
| __bench_uretprobe | Kernel | 1635.32 | 1651.28 | 1641.26 | 4.32 |
| __bench_uretprobe | Userspace | 266.16 | 268.43 | 267.29 | 0.78 |
| __bench_write | Kernel | 17662.12 | 17731.32 | 17700.15 | 23.20 |
| __bench_write | Userspace | 4268.04 | 4271.13 | 4269.84 | 1.05 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.97 | 174.99 | 157.57 | 6.51 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-18 21:10:44
- **Total duration:** 1500.06 seconds

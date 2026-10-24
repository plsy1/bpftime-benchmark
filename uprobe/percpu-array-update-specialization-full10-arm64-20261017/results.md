# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-17 22:33:12*

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
| __bench_uprobe | 1227.00 | 194.91 | 6.30x |
| __bench_uretprobe | 1645.38 | 265.68 | 6.19x |
| __bench_uprobe_uretprobe | 1757.45 | 318.93 | 5.51x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3682.36 | 3697.18 | 3687.83 | 4.82 |
| __bench_array_map_delete | Userspace | 20003.69 | 20204.60 | 20149.06 | 70.78 |
| __bench_array_map_lookup | Kernel | 3808.36 | 3816.01 | 3810.68 | 2.16 |
| __bench_array_map_lookup | Userspace | 7906.96 | 7974.00 | 7950.52 | 18.05 |
| __bench_array_map_update | Kernel | 13184.83 | 13232.82 | 13204.22 | 14.20 |
| __bench_array_map_update | Userspace | 11524.23 | 11571.99 | 11542.24 | 15.16 |
| __bench_hash_map_delete | Kernel | 106347.12 | 106506.29 | 106433.94 | 43.00 |
| __bench_hash_map_delete | Userspace | 36601.28 | 37218.33 | 36936.70 | 179.61 |
| __bench_hash_map_lookup | Kernel | 28265.87 | 28562.31 | 28379.33 | 102.27 |
| __bench_hash_map_lookup | Userspace | 29458.83 | 30353.88 | 29611.47 | 261.20 |
| __bench_hash_map_update | Kernel | 93524.66 | 93706.51 | 93622.52 | 57.61 |
| __bench_hash_map_update | Userspace | 53049.16 | 54200.24 | 53603.07 | 364.96 |
| __bench_per_cpu_array_map_delete | Kernel | 3690.39 | 3727.53 | 3697.82 | 10.33 |
| __bench_per_cpu_array_map_delete | Userspace | 20077.06 | 20239.29 | 20174.18 | 58.55 |
| __bench_per_cpu_array_map_lookup | Kernel | 4404.44 | 4471.82 | 4414.64 | 19.19 |
| __bench_per_cpu_array_map_lookup | Userspace | 11603.48 | 11721.26 | 11656.96 | 41.82 |
| __bench_per_cpu_array_map_update | Kernel | 16562.08 | 16610.25 | 16585.36 | 15.23 |
| __bench_per_cpu_array_map_update | Userspace | 14276.53 | 14493.18 | 14341.95 | 71.26 |
| __bench_per_cpu_hash_map_delete | Kernel | 151440.48 | 151645.00 | 151516.52 | 58.44 |
| __bench_per_cpu_hash_map_delete | Userspace | 99291.47 | 102473.31 | 99960.84 | 852.75 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29585.56 | 29745.20 | 29679.83 | 54.48 |
| __bench_per_cpu_hash_map_lookup | Userspace | 41675.72 | 41824.40 | 41753.25 | 47.90 |
| __bench_per_cpu_hash_map_update | Kernel | 67605.07 | 67804.61 | 67713.02 | 65.16 |
| __bench_per_cpu_hash_map_update | Userspace | 63609.49 | 65917.35 | 63911.29 | 670.58 |
| __bench_read | Kernel | 18075.25 | 18196.65 | 18134.80 | 34.78 |
| __bench_read | Userspace | 4581.57 | 4600.85 | 4589.50 | 6.11 |
| __bench_uprobe | Kernel | 1225.23 | 1228.72 | 1227.00 | 1.19 |
| __bench_uprobe | Userspace | 192.79 | 196.86 | 194.91 | 1.42 |
| __bench_uprobe_uretprobe | Kernel | 1747.76 | 1781.91 | 1757.45 | 11.95 |
| __bench_uprobe_uretprobe | Userspace | 317.68 | 320.14 | 318.93 | 0.87 |
| __bench_uretprobe | Kernel | 1637.33 | 1688.50 | 1645.38 | 14.73 |
| __bench_uretprobe | Userspace | 264.33 | 267.50 | 265.68 | 0.92 |
| __bench_write | Kernel | 17604.26 | 17833.63 | 17668.23 | 66.89 |
| __bench_write | Userspace | 4269.65 | 4272.59 | 4271.38 | 1.05 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 157.69 | 316.94 | 175.80 | 47.12 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-17 22:33:12
- **Total duration:** 1594.15 seconds

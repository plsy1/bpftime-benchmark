# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-16 16:19:48*

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
| __bench_uprobe | 1231.92 | 197.91 | 6.22x |
| __bench_uretprobe | 1646.30 | 267.55 | 6.15x |
| __bench_uprobe_uretprobe | 1757.34 | 319.60 | 5.50x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3685.27 | 3691.66 | 3687.67 | 1.99 |
| __bench_array_map_delete | Userspace | 20056.27 | 20286.71 | 20173.35 | 54.70 |
| __bench_array_map_lookup | Kernel | 3806.71 | 3816.74 | 3810.49 | 3.13 |
| __bench_array_map_lookup | Userspace | 10870.94 | 10909.27 | 10883.13 | 12.32 |
| __bench_array_map_update | Kernel | 13067.18 | 13094.27 | 13082.16 | 8.03 |
| __bench_array_map_update | Userspace | 13897.43 | 14291.33 | 14017.19 | 132.63 |
| __bench_hash_map_delete | Kernel | 107743.31 | 107876.65 | 107823.74 | 48.21 |
| __bench_hash_map_delete | Userspace | 47047.25 | 48155.05 | 47420.85 | 330.86 |
| __bench_hash_map_lookup | Kernel | 28117.81 | 28488.63 | 28271.64 | 91.18 |
| __bench_hash_map_lookup | Userspace | 49060.70 | 50708.35 | 49792.59 | 384.29 |
| __bench_hash_map_update | Kernel | 93327.48 | 93594.12 | 93480.45 | 79.28 |
| __bench_hash_map_update | Userspace | 62159.15 | 63084.80 | 62596.12 | 248.99 |
| __bench_per_cpu_array_map_delete | Kernel | 3682.91 | 3703.07 | 3690.45 | 5.98 |
| __bench_per_cpu_array_map_delete | Userspace | 20038.82 | 20190.81 | 20143.64 | 38.95 |
| __bench_per_cpu_array_map_lookup | Kernel | 4401.71 | 4412.48 | 4406.64 | 3.91 |
| __bench_per_cpu_array_map_lookup | Userspace | 14648.84 | 14792.12 | 14748.79 | 42.06 |
| __bench_per_cpu_array_map_update | Kernel | 16624.39 | 16707.03 | 16653.80 | 22.67 |
| __bench_per_cpu_array_map_update | Userspace | 27498.72 | 27925.07 | 27649.80 | 117.14 |
| __bench_per_cpu_hash_map_delete | Kernel | 152143.43 | 152348.08 | 152260.43 | 52.41 |
| __bench_per_cpu_hash_map_delete | Userspace | 99288.58 | 102251.86 | 100187.13 | 1032.07 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29290.93 | 29585.74 | 29419.24 | 87.66 |
| __bench_per_cpu_hash_map_lookup | Userspace | 44883.46 | 45126.01 | 45008.50 | 69.20 |
| __bench_per_cpu_hash_map_update | Kernel | 68427.88 | 68575.28 | 68491.21 | 48.38 |
| __bench_per_cpu_hash_map_update | Userspace | 60827.16 | 61470.20 | 61202.28 | 159.18 |
| __bench_read | Kernel | 18059.04 | 18226.35 | 18114.34 | 46.63 |
| __bench_read | Userspace | 4557.65 | 4563.57 | 4560.30 | 1.83 |
| __bench_uprobe | Kernel | 1224.64 | 1241.73 | 1231.92 | 5.95 |
| __bench_uprobe | Userspace | 195.06 | 202.43 | 197.91 | 2.51 |
| __bench_uprobe_uretprobe | Kernel | 1748.29 | 1779.32 | 1757.34 | 9.35 |
| __bench_uprobe_uretprobe | Userspace | 316.63 | 323.52 | 319.60 | 2.21 |
| __bench_uretprobe | Kernel | 1637.09 | 1661.49 | 1646.30 | 8.48 |
| __bench_uretprobe | Userspace | 266.40 | 268.92 | 267.55 | 0.79 |
| __bench_write | Kernel | 17630.33 | 17693.01 | 17673.69 | 17.71 |
| __bench_write | Userspace | 4269.90 | 4277.53 | 4273.01 | 2.49 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.90 | 181.67 | 159.75 | 7.94 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-16 16:19:48
- **Total duration:** 1737.89 seconds

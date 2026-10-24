# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-16 13:09:39*

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
| __bench_uprobe | 1227.05 | 195.74 | 6.27x |
| __bench_uretprobe | 1636.56 | 273.00 | 5.99x |
| __bench_uprobe_uretprobe | 1754.14 | 318.64 | 5.51x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3663.59 | 3686.84 | 3677.64 | 5.80 |
| __bench_array_map_delete | Userspace | 20048.45 | 20295.94 | 20197.27 | 76.13 |
| __bench_array_map_lookup | Kernel | 3780.20 | 3793.21 | 3783.18 | 4.08 |
| __bench_array_map_lookup | Userspace | 10823.71 | 10875.34 | 10857.01 | 14.41 |
| __bench_array_map_update | Kernel | 13070.74 | 13103.66 | 13088.45 | 9.39 |
| __bench_array_map_update | Userspace | 13903.18 | 14261.96 | 14024.35 | 87.88 |
| __bench_hash_map_delete | Kernel | 108004.77 | 108114.54 | 108048.15 | 38.14 |
| __bench_hash_map_delete | Userspace | 47001.11 | 47537.73 | 47298.33 | 191.02 |
| __bench_hash_map_lookup | Kernel | 28622.14 | 28870.06 | 28774.80 | 67.81 |
| __bench_hash_map_lookup | Userspace | 49207.18 | 52384.35 | 50318.45 | 870.22 |
| __bench_hash_map_update | Kernel | 94345.73 | 94468.21 | 94394.28 | 34.50 |
| __bench_hash_map_update | Userspace | 62550.82 | 63317.88 | 62758.04 | 224.95 |
| __bench_per_cpu_array_map_delete | Kernel | 3683.12 | 3698.04 | 3688.06 | 5.17 |
| __bench_per_cpu_array_map_delete | Userspace | 20048.13 | 20179.46 | 20139.78 | 34.00 |
| __bench_per_cpu_array_map_lookup | Kernel | 4400.15 | 4410.63 | 4404.65 | 3.45 |
| __bench_per_cpu_array_map_lookup | Userspace | 14683.21 | 14773.73 | 14733.95 | 29.93 |
| __bench_per_cpu_array_map_update | Kernel | 16601.66 | 16646.86 | 16624.04 | 13.10 |
| __bench_per_cpu_array_map_update | Userspace | 27509.32 | 27823.47 | 27628.38 | 78.13 |
| __bench_per_cpu_hash_map_delete | Kernel | 152492.96 | 152594.59 | 152547.20 | 37.41 |
| __bench_per_cpu_hash_map_delete | Userspace | 99253.56 | 100025.98 | 99764.86 | 260.79 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29808.09 | 30007.66 | 29905.79 | 61.67 |
| __bench_per_cpu_hash_map_lookup | Userspace | 49668.58 | 50191.13 | 49897.17 | 151.64 |
| __bench_per_cpu_hash_map_update | Kernel | 68521.81 | 68595.98 | 68557.54 | 23.39 |
| __bench_per_cpu_hash_map_update | Userspace | 61009.60 | 61233.28 | 61157.72 | 72.10 |
| __bench_read | Kernel | 18059.61 | 18297.67 | 18128.84 | 62.63 |
| __bench_read | Userspace | 4571.32 | 4594.78 | 4582.18 | 6.77 |
| __bench_uprobe | Kernel | 1225.72 | 1228.17 | 1227.05 | 0.76 |
| __bench_uprobe | Userspace | 193.76 | 199.31 | 195.74 | 1.57 |
| __bench_uprobe_uretprobe | Kernel | 1750.16 | 1758.75 | 1754.14 | 2.95 |
| __bench_uprobe_uretprobe | Userspace | 317.25 | 321.35 | 318.64 | 1.40 |
| __bench_uretprobe | Kernel | 1632.62 | 1648.36 | 1636.56 | 5.61 |
| __bench_uretprobe | Userspace | 265.10 | 289.97 | 273.00 | 10.72 |
| __bench_write | Kernel | 17581.30 | 17771.52 | 17682.15 | 53.91 |
| __bench_write | Userspace | 4269.95 | 4621.23 | 4442.22 | 154.87 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 153.74 | 341.84 | 174.98 | 55.68 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-16 13:09:39
- **Total duration:** 1744.68 seconds

# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-21 23:15:33*

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
| __bench_uprobe | 1230.17 | 198.84 | 6.19x |
| __bench_uretprobe | 1645.59 | 268.60 | 6.13x |
| __bench_uprobe_uretprobe | 1756.73 | 323.15 | 5.44x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3683.75 | 3699.54 | 3688.49 | 4.71 |
| __bench_array_map_delete | Userspace | 20074.21 | 20297.68 | 20205.60 | 81.62 |
| __bench_array_map_lookup | Kernel | 3738.05 | 3749.73 | 3741.77 | 3.63 |
| __bench_array_map_lookup | Userspace | 1165.59 | 1173.75 | 1170.38 | 2.65 |
| __bench_array_map_update | Kernel | 13064.98 | 13104.42 | 13082.99 | 10.89 |
| __bench_array_map_update | Userspace | 11527.28 | 11823.03 | 11605.66 | 102.46 |
| __bench_hash_map_delete | Kernel | 106955.46 | 107075.60 | 107017.45 | 36.46 |
| __bench_hash_map_delete | Userspace | 36174.65 | 36391.03 | 36279.15 | 76.06 |
| __bench_hash_map_lookup | Kernel | 27738.80 | 28000.31 | 27859.57 | 76.33 |
| __bench_hash_map_lookup | Userspace | 26951.90 | 27704.12 | 27219.55 | 209.34 |
| __bench_hash_map_update | Kernel | 93819.44 | 93953.48 | 93920.40 | 37.99 |
| __bench_hash_map_update | Userspace | 33012.02 | 43013.58 | 35413.11 | 2817.83 |
| __bench_per_cpu_array_map_delete | Kernel | 3683.50 | 3694.49 | 3687.26 | 4.07 |
| __bench_per_cpu_array_map_delete | Userspace | 20204.03 | 20383.92 | 20317.07 | 71.71 |
| __bench_per_cpu_array_map_lookup | Kernel | 4401.45 | 4415.63 | 4405.65 | 4.68 |
| __bench_per_cpu_array_map_lookup | Userspace | 4094.39 | 4123.16 | 4109.20 | 9.89 |
| __bench_per_cpu_array_map_update | Kernel | 16575.72 | 16675.66 | 16622.68 | 31.71 |
| __bench_per_cpu_array_map_update | Userspace | 14185.25 | 14655.73 | 14378.47 | 143.72 |
| __bench_per_cpu_hash_map_delete | Kernel | 151408.80 | 151561.21 | 151491.84 | 48.55 |
| __bench_per_cpu_hash_map_delete | Userspace | 98294.94 | 99292.79 | 98822.00 | 371.71 |
| __bench_per_cpu_hash_map_lookup | Kernel | 28935.80 | 29038.08 | 28992.57 | 34.27 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27600.55 | 27865.46 | 27756.66 | 90.90 |
| __bench_per_cpu_hash_map_update | Kernel | 67792.51 | 67895.89 | 67847.48 | 25.92 |
| __bench_per_cpu_hash_map_update | Userspace | 44316.94 | 44646.85 | 44472.24 | 80.86 |
| __bench_read | Kernel | 18077.56 | 18185.59 | 18129.22 | 33.04 |
| __bench_read | Userspace | 4567.25 | 4572.24 | 4569.14 | 1.80 |
| __bench_uprobe | Kernel | 1225.07 | 1234.81 | 1230.17 | 3.59 |
| __bench_uprobe | Userspace | 196.75 | 200.89 | 198.84 | 1.13 |
| __bench_uprobe_uretprobe | Kernel | 1750.93 | 1767.44 | 1756.73 | 5.23 |
| __bench_uprobe_uretprobe | Userspace | 322.37 | 325.53 | 323.15 | 0.95 |
| __bench_uretprobe | Kernel | 1637.32 | 1652.30 | 1645.59 | 5.76 |
| __bench_uretprobe | Userspace | 267.11 | 270.98 | 268.60 | 1.00 |
| __bench_write | Kernel | 17759.07 | 20990.57 | 18117.97 | 957.78 |
| __bench_write | Userspace | 4270.61 | 4276.50 | 4273.09 | 1.91 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.74 | 175.87 | 157.76 | 6.36 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-21 23:15:33
- **Total duration:** 1486.73 seconds

# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-21 13:58:41*

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
| __bench_uprobe | 1230.66 | 198.19 | 6.21x |
| __bench_uretprobe | 1644.35 | 268.80 | 6.12x |
| __bench_uprobe_uretprobe | 1756.66 | 322.60 | 5.45x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3683.46 | 3702.84 | 3689.78 | 6.43 |
| __bench_array_map_delete | Userspace | 20519.82 | 20823.60 | 20676.70 | 122.02 |
| __bench_array_map_lookup | Kernel | 3804.67 | 3822.80 | 3810.31 | 4.65 |
| __bench_array_map_lookup | Userspace | 1162.42 | 1179.95 | 1172.08 | 4.89 |
| __bench_array_map_update | Kernel | 13200.85 | 13232.41 | 13214.20 | 10.97 |
| __bench_array_map_update | Userspace | 11485.55 | 11598.12 | 11528.06 | 34.85 |
| __bench_hash_map_delete | Kernel | 107780.93 | 107988.86 | 107884.08 | 60.53 |
| __bench_hash_map_delete | Userspace | 43438.01 | 43892.78 | 43689.60 | 152.50 |
| __bench_hash_map_lookup | Kernel | 28901.27 | 29170.92 | 29063.49 | 78.14 |
| __bench_hash_map_lookup | Userspace | 26973.53 | 27285.35 | 27188.28 | 129.20 |
| __bench_hash_map_update | Kernel | 93095.82 | 93301.06 | 93197.40 | 57.56 |
| __bench_hash_map_update | Userspace | 32999.71 | 35885.09 | 35068.83 | 1187.26 |
| __bench_per_cpu_array_map_delete | Kernel | 3684.49 | 3697.77 | 3688.66 | 4.16 |
| __bench_per_cpu_array_map_delete | Userspace | 20539.76 | 20862.74 | 20698.38 | 125.08 |
| __bench_per_cpu_array_map_lookup | Kernel | 4397.46 | 4413.12 | 4406.52 | 4.59 |
| __bench_per_cpu_array_map_lookup | Userspace | 4106.42 | 4125.18 | 4112.80 | 6.98 |
| __bench_per_cpu_array_map_update | Kernel | 16568.05 | 16624.01 | 16595.96 | 16.60 |
| __bench_per_cpu_array_map_update | Userspace | 14347.89 | 14443.86 | 14393.92 | 28.15 |
| __bench_per_cpu_hash_map_delete | Kernel | 151822.88 | 151940.23 | 151893.28 | 33.74 |
| __bench_per_cpu_hash_map_delete | Userspace | 99266.44 | 102716.94 | 99811.33 | 980.72 |
| __bench_per_cpu_hash_map_lookup | Kernel | 28764.72 | 28997.08 | 28853.90 | 63.66 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27629.57 | 28043.35 | 27744.81 | 122.34 |
| __bench_per_cpu_hash_map_update | Kernel | 67811.11 | 67886.65 | 67835.35 | 20.52 |
| __bench_per_cpu_hash_map_update | Userspace | 44398.35 | 44766.86 | 44502.01 | 100.63 |
| __bench_read | Kernel | 18030.81 | 18179.32 | 18092.52 | 45.99 |
| __bench_read | Userspace | 4567.19 | 4574.58 | 4570.74 | 2.16 |
| __bench_uprobe | Kernel | 1225.68 | 1240.24 | 1230.66 | 4.23 |
| __bench_uprobe | Userspace | 197.01 | 199.62 | 198.19 | 0.90 |
| __bench_uprobe_uretprobe | Kernel | 1749.56 | 1770.24 | 1756.66 | 5.64 |
| __bench_uprobe_uretprobe | Userspace | 321.70 | 325.19 | 322.60 | 0.97 |
| __bench_uretprobe | Kernel | 1638.62 | 1657.93 | 1644.35 | 5.57 |
| __bench_uretprobe | Userspace | 267.47 | 271.79 | 268.80 | 1.17 |
| __bench_write | Kernel | 17556.44 | 17780.12 | 17687.37 | 62.78 |
| __bench_write | Userspace | 4272.16 | 4275.53 | 4273.39 | 0.91 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 153.24 | 168.68 | 156.24 | 4.45 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-21 13:58:41
- **Total duration:** 1494.44 seconds

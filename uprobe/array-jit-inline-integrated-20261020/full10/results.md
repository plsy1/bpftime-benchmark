# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-20 01:26:29*

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
| __bench_uprobe | 1228.40 | 198.60 | 6.19x |
| __bench_uretprobe | 1646.45 | 269.62 | 6.11x |
| __bench_uprobe_uretprobe | 1758.98 | 324.64 | 5.42x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3681.91 | 3736.87 | 3689.77 | 15.93 |
| __bench_array_map_delete | Userspace | 20622.57 | 22549.89 | 20916.04 | 548.21 |
| __bench_array_map_lookup | Kernel | 3784.16 | 3805.54 | 3788.70 | 6.07 |
| __bench_array_map_lookup | Userspace | 1167.28 | 1177.55 | 1171.83 | 3.28 |
| __bench_array_map_update | Kernel | 13064.22 | 13090.54 | 13080.71 | 8.19 |
| __bench_array_map_update | Userspace | 11495.40 | 11731.97 | 11546.73 | 65.69 |
| __bench_hash_map_delete | Kernel | 108814.99 | 108963.34 | 108888.11 | 47.93 |
| __bench_hash_map_delete | Userspace | 36996.87 | 37434.09 | 37167.83 | 148.70 |
| __bench_hash_map_lookup | Kernel | 27588.85 | 27887.00 | 27773.85 | 93.12 |
| __bench_hash_map_lookup | Userspace | 26820.27 | 27349.46 | 26975.53 | 164.64 |
| __bench_hash_map_update | Kernel | 95341.12 | 95495.69 | 95406.49 | 45.65 |
| __bench_hash_map_update | Userspace | 33012.66 | 35987.95 | 34301.66 | 1300.22 |
| __bench_per_cpu_array_map_delete | Kernel | 3683.55 | 3712.11 | 3692.59 | 8.88 |
| __bench_per_cpu_array_map_delete | Userspace | 20688.93 | 20827.17 | 20783.16 | 49.24 |
| __bench_per_cpu_array_map_lookup | Kernel | 4407.21 | 4414.74 | 4410.75 | 2.70 |
| __bench_per_cpu_array_map_lookup | Userspace | 10856.76 | 10897.97 | 10875.20 | 12.50 |
| __bench_per_cpu_array_map_update | Kernel | 16567.99 | 16625.71 | 16597.49 | 17.00 |
| __bench_per_cpu_array_map_update | Userspace | 14459.18 | 14718.27 | 14519.41 | 74.10 |
| __bench_per_cpu_hash_map_delete | Kernel | 151884.06 | 151957.36 | 151923.38 | 24.80 |
| __bench_per_cpu_hash_map_delete | Userspace | 99521.88 | 100399.26 | 99858.86 | 357.41 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29669.88 | 29859.18 | 29753.57 | 56.71 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27595.32 | 27875.32 | 27721.93 | 82.78 |
| __bench_per_cpu_hash_map_update | Kernel | 67771.49 | 67858.55 | 67824.15 | 22.35 |
| __bench_per_cpu_hash_map_update | Userspace | 44303.36 | 44552.37 | 44437.83 | 70.01 |
| __bench_read | Kernel | 18039.65 | 18163.03 | 18120.95 | 33.71 |
| __bench_read | Userspace | 4571.11 | 4579.45 | 4575.76 | 2.72 |
| __bench_uprobe | Kernel | 1224.76 | 1234.13 | 1228.40 | 3.65 |
| __bench_uprobe | Userspace | 196.98 | 200.10 | 198.60 | 0.85 |
| __bench_uprobe_uretprobe | Kernel | 1750.30 | 1764.70 | 1758.98 | 4.00 |
| __bench_uprobe_uretprobe | Userspace | 323.34 | 326.03 | 324.64 | 0.82 |
| __bench_uretprobe | Kernel | 1638.12 | 1660.51 | 1646.45 | 6.00 |
| __bench_uretprobe | Userspace | 267.92 | 272.18 | 269.62 | 1.29 |
| __bench_write | Kernel | 17592.16 | 17740.67 | 17681.14 | 47.03 |
| __bench_write | Userspace | 4273.15 | 4850.93 | 4446.15 | 260.57 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.73 | 172.00 | 156.98 | 5.55 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-20 01:26:29
- **Total duration:** 1495.79 seconds

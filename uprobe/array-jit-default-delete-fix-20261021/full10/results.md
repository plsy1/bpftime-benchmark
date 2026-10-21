# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-21 14:59:04*

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
| __bench_uprobe | 1228.48 | 199.75 | 6.15x |
| __bench_uretprobe | 1647.63 | 270.07 | 6.10x |
| __bench_uprobe_uretprobe | 1762.04 | 324.32 | 5.43x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3677.87 | 3706.43 | 3685.02 | 7.78 |
| __bench_array_map_delete | Userspace | 20081.20 | 20281.26 | 20181.50 | 67.67 |
| __bench_array_map_lookup | Kernel | 3738.11 | 3772.13 | 3744.02 | 10.13 |
| __bench_array_map_lookup | Userspace | 1167.11 | 1189.22 | 1175.90 | 6.53 |
| __bench_array_map_update | Kernel | 13019.15 | 13092.19 | 13044.36 | 20.48 |
| __bench_array_map_update | Userspace | 11494.54 | 11720.86 | 11550.00 | 61.00 |
| __bench_hash_map_delete | Kernel | 107945.68 | 108133.09 | 108055.29 | 66.39 |
| __bench_hash_map_delete | Userspace | 36191.83 | 39796.78 | 36638.71 | 1054.46 |
| __bench_hash_map_lookup | Kernel | 28766.31 | 29225.15 | 29004.14 | 125.47 |
| __bench_hash_map_lookup | Userspace | 26798.95 | 27086.32 | 26899.07 | 109.25 |
| __bench_hash_map_update | Kernel | 94765.11 | 95083.29 | 94950.50 | 94.21 |
| __bench_hash_map_update | Userspace | 32961.03 | 36395.91 | 34208.16 | 1398.19 |
| __bench_per_cpu_array_map_delete | Kernel | 3682.77 | 3697.86 | 3687.14 | 5.16 |
| __bench_per_cpu_array_map_delete | Userspace | 20179.00 | 20391.67 | 20302.50 | 82.30 |
| __bench_per_cpu_array_map_lookup | Kernel | 4403.01 | 4415.08 | 4406.74 | 3.26 |
| __bench_per_cpu_array_map_lookup | Userspace | 4104.49 | 4128.41 | 4115.23 | 8.39 |
| __bench_per_cpu_array_map_update | Kernel | 16555.56 | 16634.10 | 16584.86 | 25.20 |
| __bench_per_cpu_array_map_update | Userspace | 14335.34 | 14562.84 | 14409.66 | 63.07 |
| __bench_per_cpu_hash_map_delete | Kernel | 149444.53 | 149633.32 | 149542.98 | 63.93 |
| __bench_per_cpu_hash_map_delete | Userspace | 98360.82 | 99417.11 | 98766.10 | 367.56 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29268.82 | 29442.34 | 29374.76 | 53.49 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27577.22 | 27907.08 | 27738.47 | 95.47 |
| __bench_per_cpu_hash_map_update | Kernel | 67988.79 | 68188.65 | 68107.13 | 59.97 |
| __bench_per_cpu_hash_map_update | Userspace | 44371.59 | 44502.63 | 44420.13 | 37.06 |
| __bench_read | Kernel | 17618.40 | 17689.59 | 17644.15 | 21.53 |
| __bench_read | Userspace | 4563.99 | 4574.31 | 4568.36 | 2.70 |
| __bench_uprobe | Kernel | 1224.49 | 1232.38 | 1228.48 | 2.17 |
| __bench_uprobe | Userspace | 197.17 | 206.18 | 199.75 | 2.73 |
| __bench_uprobe_uretprobe | Kernel | 1748.78 | 1779.81 | 1762.04 | 10.87 |
| __bench_uprobe_uretprobe | Userspace | 322.76 | 327.76 | 324.32 | 1.61 |
| __bench_uretprobe | Kernel | 1636.87 | 1662.46 | 1647.63 | 8.51 |
| __bench_uretprobe | Userspace | 267.77 | 275.17 | 270.07 | 2.07 |
| __bench_write | Kernel | 17739.22 | 17880.87 | 17791.27 | 36.77 |
| __bench_write | Userspace | 4273.56 | 4849.52 | 4551.37 | 242.16 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 153.03 | 171.26 | 157.76 | 5.48 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-21 14:59:04
- **Total duration:** 1480.86 seconds

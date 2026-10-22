# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-22 15:26:59*

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

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Userspace | 20075.80 | 20347.91 | 20199.68 | 99.57 |
| __bench_array_map_lookup | Userspace | 1165.70 | 1173.32 | 1169.68 | 2.76 |
| __bench_array_map_update | Userspace | 11512.23 | 11600.12 | 11545.24 | 30.14 |
| __bench_hash_map_delete | Userspace | 36033.53 | 36390.33 | 36242.24 | 92.60 |
| __bench_hash_map_lookup | Userspace | 26798.07 | 27088.72 | 26980.08 | 110.99 |
| __bench_hash_map_update | Userspace | 33003.11 | 36336.25 | 34918.42 | 1336.78 |
| __bench_per_cpu_array_map_delete | Userspace | 20184.38 | 20438.02 | 20305.79 | 92.23 |
| __bench_per_cpu_array_map_lookup | Userspace | 4096.94 | 4117.43 | 4104.90 | 5.68 |
| __bench_per_cpu_array_map_update | Userspace | 14483.40 | 14609.62 | 14532.96 | 40.71 |
| __bench_per_cpu_hash_map_delete | Userspace | 98296.13 | 99319.04 | 98844.17 | 376.20 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27569.19 | 27853.36 | 27731.51 | 76.40 |
| __bench_per_cpu_hash_map_update | Userspace | 44290.51 | 44452.11 | 44375.76 | 50.01 |
| __bench_read | Userspace | 4565.43 | 4568.95 | 4566.56 | 1.00 |
| __bench_uprobe | Userspace | 196.93 | 200.16 | 198.71 | 0.93 |
| __bench_uprobe_uretprobe | Userspace | 323.17 | 326.67 | 324.54 | 1.16 |
| __bench_uretprobe | Userspace | 269.53 | 275.84 | 271.11 | 1.92 |
| __bench_write | Userspace | 4272.94 | 4848.24 | 4525.46 | 270.16 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 153.38 | 170.92 | 157.87 | 4.92 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-22 15:26:59
- **Total duration:** 603.56 seconds

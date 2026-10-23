# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-23 13:57:45*

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
| __bench_array_map_delete | Userspace | 20525.94 | 20783.64 | 20690.15 | 103.25 |
| __bench_array_map_lookup | Userspace | 1166.17 | 1182.22 | 1171.50 | 4.08 |
| __bench_array_map_update | Userspace | 11490.91 | 11565.25 | 11530.40 | 23.48 |
| __bench_hash_map_delete | Userspace | 27176.47 | 28772.67 | 27941.50 | 603.01 |
| __bench_hash_map_lookup | Userspace | 23584.62 | 25002.40 | 24222.83 | 607.29 |
| __bench_hash_map_update | Userspace | 27232.42 | 31289.39 | 29290.08 | 1633.57 |
| __bench_per_cpu_array_map_delete | Userspace | 20548.20 | 20827.83 | 20720.51 | 108.85 |
| __bench_per_cpu_array_map_lookup | Userspace | 4097.30 | 4125.41 | 4115.13 | 8.45 |
| __bench_per_cpu_array_map_update | Userspace | 14423.89 | 14647.10 | 14494.97 | 69.16 |
| __bench_per_cpu_hash_map_delete | Userspace | 98989.06 | 99821.09 | 99278.44 | 205.57 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27649.17 | 27889.74 | 27773.25 | 98.24 |
| __bench_per_cpu_hash_map_update | Userspace | 44452.14 | 44797.68 | 44548.26 | 94.73 |
| __bench_read | Userspace | 4564.97 | 4569.84 | 4567.50 | 1.62 |
| __bench_uprobe | Userspace | 195.56 | 198.95 | 197.39 | 1.04 |
| __bench_uprobe_uretprobe | Userspace | 322.23 | 328.04 | 323.77 | 1.64 |
| __bench_uretprobe | Userspace | 267.16 | 271.66 | 268.86 | 1.28 |
| __bench_write | Userspace | 4270.98 | 4275.54 | 4273.18 | 1.53 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 153.57 | 170.76 | 157.25 | 4.81 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-23 13:57:45
- **Total duration:** 520.12 seconds

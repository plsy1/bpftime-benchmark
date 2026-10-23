# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-23 18:05:24*

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
| __bench_array_map_delete | Userspace | 20828.94 | 21016.38 | 20943.60 | 77.78 |
| __bench_array_map_lookup | Userspace | 1165.22 | 1180.50 | 1173.95 | 4.14 |
| __bench_array_map_update | Userspace | 11491.93 | 11725.51 | 11569.70 | 74.52 |
| __bench_hash_map_delete | Userspace | 27298.88 | 28475.22 | 27785.31 | 459.30 |
| __bench_hash_map_lookup | Userspace | 23863.79 | 25034.82 | 24555.06 | 447.32 |
| __bench_hash_map_update | Userspace | 27669.80 | 31430.01 | 30146.48 | 1512.46 |
| __bench_per_cpu_array_map_delete | Userspace | 20773.31 | 20975.12 | 20896.25 | 79.21 |
| __bench_per_cpu_array_map_lookup | Userspace | 4099.99 | 4278.40 | 4129.68 | 50.56 |
| __bench_per_cpu_array_map_update | Userspace | 14219.38 | 14599.35 | 14331.78 | 121.07 |
| __bench_per_cpu_hash_map_delete | Userspace | 98822.00 | 99550.61 | 99131.11 | 202.13 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27628.10 | 27856.36 | 27742.42 | 78.10 |
| __bench_per_cpu_hash_map_update | Userspace | 44286.95 | 44582.74 | 44466.32 | 90.56 |
| __bench_read | Userspace | 4584.03 | 4593.87 | 4588.85 | 3.45 |
| __bench_uprobe | Userspace | 199.00 | 204.09 | 201.02 | 1.63 |
| __bench_uprobe_uretprobe | Userspace | 323.82 | 331.46 | 326.11 | 2.04 |
| __bench_uretprobe | Userspace | 267.04 | 274.43 | 269.09 | 2.11 |
| __bench_write | Userspace | 4273.06 | 4281.68 | 4276.11 | 2.35 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 155.50 | 176.93 | 160.42 | 6.01 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-23 18:05:24
- **Total duration:** 524.97 seconds

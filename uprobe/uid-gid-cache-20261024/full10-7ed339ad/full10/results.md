# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-24 15:58:44*

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
| __bench_array_map_delete | Userspace | 20901.24 | 21093.46 | 20989.10 | 75.74 |
| __bench_array_map_lookup | Userspace | 1162.24 | 1171.49 | 1168.16 | 2.94 |
| __bench_array_map_update | Userspace | 11472.73 | 11697.70 | 11518.99 | 62.14 |
| __bench_hash_map_delete | Userspace | 27230.04 | 28457.49 | 27667.92 | 523.09 |
| __bench_hash_map_lookup | Userspace | 23828.07 | 25425.41 | 24689.91 | 466.69 |
| __bench_hash_map_update | Userspace | 27739.08 | 33023.32 | 30592.02 | 1572.24 |
| __bench_per_cpu_array_map_delete | Userspace | 20884.52 | 21061.55 | 20964.84 | 63.99 |
| __bench_per_cpu_array_map_lookup | Userspace | 4092.89 | 4117.29 | 4103.80 | 9.15 |
| __bench_per_cpu_array_map_update | Userspace | 14179.82 | 14620.50 | 14292.39 | 127.32 |
| __bench_per_cpu_hash_map_delete | Userspace | 100082.27 | 100292.00 | 100177.90 | 56.42 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27589.00 | 27783.51 | 27694.31 | 60.03 |
| __bench_per_cpu_hash_map_update | Userspace | 44421.40 | 44593.20 | 44549.30 | 50.36 |
| __bench_read | Userspace | 4571.79 | 4579.80 | 4576.65 | 2.20 |
| __bench_uprobe | Userspace | 196.50 | 201.20 | 198.40 | 1.62 |
| __bench_uprobe_uretprobe | Userspace | 322.19 | 326.61 | 323.35 | 1.30 |
| __bench_uretprobe | Userspace | 264.59 | 270.30 | 266.67 | 1.69 |
| __bench_write | Userspace | 4269.91 | 4273.63 | 4272.01 | 1.40 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 155.06 | 180.38 | 161.12 | 7.19 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-24 15:58:44
- **Total duration:** 525.16 seconds

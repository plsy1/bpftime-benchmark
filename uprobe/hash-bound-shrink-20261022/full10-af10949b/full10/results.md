# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-22 22:45:42*

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
| __bench_array_map_delete | Userspace | 20076.01 | 20291.22 | 20162.82 | 87.42 |
| __bench_array_map_lookup | Userspace | 1172.03 | 1175.57 | 1174.01 | 1.09 |
| __bench_array_map_update | Userspace | 11538.26 | 11600.88 | 11557.25 | 15.52 |
| __bench_hash_map_delete | Userspace | 38350.80 | 41845.97 | 39230.67 | 1150.80 |
| __bench_hash_map_lookup | Userspace | 23753.91 | 24871.07 | 24114.43 | 491.55 |
| __bench_hash_map_update | Userspace | 27143.58 | 30827.80 | 28286.42 | 1603.66 |
| __bench_per_cpu_array_map_delete | Userspace | 20189.65 | 20375.98 | 20264.22 | 87.09 |
| __bench_per_cpu_array_map_lookup | Userspace | 4099.65 | 4122.45 | 4104.77 | 6.25 |
| __bench_per_cpu_array_map_update | Userspace | 14179.09 | 14335.39 | 14263.32 | 45.83 |
| __bench_per_cpu_hash_map_delete | Userspace | 98443.69 | 99373.26 | 98757.20 | 379.66 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27623.43 | 27758.25 | 27672.48 | 46.85 |
| __bench_per_cpu_hash_map_update | Userspace | 44415.09 | 44572.60 | 44475.02 | 53.80 |
| __bench_read | Userspace | 4564.31 | 4570.01 | 4567.64 | 1.51 |
| __bench_uprobe | Userspace | 196.81 | 199.90 | 198.29 | 0.82 |
| __bench_uprobe_uretprobe | Userspace | 331.56 | 340.89 | 335.01 | 3.35 |
| __bench_uretprobe | Userspace | 267.50 | 269.72 | 268.65 | 0.57 |
| __bench_write | Userspace | 4272.10 | 4836.14 | 4514.26 | 240.14 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.18 | 170.12 | 156.73 | 5.12 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-22 22:45:42
- **Total duration:** 533.31 seconds

# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-23 00:04:15*

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
| __bench_array_map_delete | Userspace | 20530.67 | 20927.09 | 20748.51 | 110.17 |
| __bench_array_map_lookup | Userspace | 1173.30 | 1184.05 | 1177.60 | 2.59 |
| __bench_array_map_update | Userspace | 11501.73 | 11609.13 | 11538.14 | 36.27 |
| __bench_hash_map_delete | Userspace | 27223.09 | 28756.21 | 28219.47 | 588.81 |
| __bench_hash_map_lookup | Userspace | 23603.42 | 25219.70 | 24360.59 | 625.66 |
| __bench_hash_map_update | Userspace | 27295.85 | 31370.30 | 29562.79 | 1781.57 |
| __bench_per_cpu_array_map_delete | Userspace | 20560.31 | 20850.46 | 20741.67 | 113.15 |
| __bench_per_cpu_array_map_lookup | Userspace | 4108.78 | 4127.96 | 4117.12 | 6.68 |
| __bench_per_cpu_array_map_update | Userspace | 14378.95 | 14514.44 | 14474.12 | 45.13 |
| __bench_per_cpu_hash_map_delete | Userspace | 99324.54 | 99530.01 | 99417.67 | 68.36 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27565.59 | 27838.32 | 27689.82 | 82.32 |
| __bench_per_cpu_hash_map_update | Userspace | 44675.64 | 44865.02 | 44771.08 | 58.30 |
| __bench_read | Userspace | 4578.29 | 4596.89 | 4589.10 | 5.46 |
| __bench_uprobe | Userspace | 204.09 | 207.15 | 205.56 | 0.94 |
| __bench_uprobe_uretprobe | Userspace | 321.61 | 323.37 | 322.47 | 0.57 |
| __bench_uretprobe | Userspace | 269.05 | 271.88 | 269.73 | 0.80 |
| __bench_write | Userspace | 4280.34 | 4282.70 | 4281.15 | 0.61 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.66 | 171.79 | 156.59 | 5.72 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-23 00:04:15
- **Total duration:** 527.99 seconds

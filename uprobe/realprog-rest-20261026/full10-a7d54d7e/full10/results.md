# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-26 02:00:55*

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
| __bench_array_map_delete | Userspace | 21023.12 | 21690.06 | 21279.98 | 189.92 |
| __bench_array_map_lookup | Userspace | 1172.52 | 1178.69 | 1175.49 | 1.63 |
| __bench_array_map_update | Userspace | 11481.79 | 11741.59 | 11540.93 | 72.74 |
| __bench_hash_map_delete | Userspace | 27322.75 | 28631.15 | 27914.91 | 549.13 |
| __bench_hash_map_lookup | Userspace | 22859.46 | 23747.06 | 23132.45 | 255.23 |
| __bench_hash_map_update | Userspace | 26512.96 | 34769.55 | 28131.17 | 2308.02 |
| __bench_per_cpu_array_map_delete | Userspace | 21066.12 | 21396.01 | 21251.14 | 114.30 |
| __bench_per_cpu_array_map_lookup | Userspace | 4105.06 | 4126.31 | 4117.55 | 7.39 |
| __bench_per_cpu_array_map_update | Userspace | 14381.30 | 14662.86 | 14510.48 | 82.37 |
| __bench_per_cpu_hash_map_delete | Userspace | 99365.22 | 99665.08 | 99598.09 | 81.35 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27598.87 | 27827.05 | 27724.99 | 66.84 |
| __bench_per_cpu_hash_map_update | Userspace | 44334.93 | 44521.48 | 44425.05 | 61.42 |
| __bench_read | Userspace | 4566.62 | 4573.22 | 4569.09 | 2.02 |
| __bench_uprobe | Userspace | 198.52 | 203.59 | 200.85 | 1.42 |
| __bench_uprobe_uretprobe | Userspace | 326.09 | 331.23 | 327.79 | 1.60 |
| __bench_uretprobe | Userspace | 272.11 | 275.93 | 273.33 | 1.09 |
| __bench_write | Userspace | 4273.00 | 4362.48 | 4288.08 | 25.58 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.31 | 175.82 | 156.91 | 6.55 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-26 02:00:55
- **Total duration:** 517.47 seconds

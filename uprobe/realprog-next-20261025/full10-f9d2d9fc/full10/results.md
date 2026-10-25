# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-25 22:19:00*

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
| __bench_array_map_delete | Userspace | 20786.25 | 21070.07 | 20901.22 | 92.62 |
| __bench_array_map_lookup | Userspace | 1166.62 | 1176.77 | 1172.96 | 2.95 |
| __bench_array_map_update | Userspace | 11512.57 | 11597.40 | 11546.04 | 28.28 |
| __bench_hash_map_delete | Userspace | 27174.23 | 28486.54 | 27927.17 | 540.59 |
| __bench_hash_map_lookup | Userspace | 22841.52 | 23465.64 | 23118.88 | 243.35 |
| __bench_hash_map_update | Userspace | 26580.81 | 28581.51 | 27704.83 | 870.98 |
| __bench_per_cpu_array_map_delete | Userspace | 20827.63 | 21142.49 | 20971.47 | 107.77 |
| __bench_per_cpu_array_map_lookup | Userspace | 4101.76 | 4131.19 | 4113.44 | 9.03 |
| __bench_per_cpu_array_map_update | Userspace | 14405.78 | 14565.63 | 14462.40 | 42.77 |
| __bench_per_cpu_hash_map_delete | Userspace | 98572.34 | 99157.63 | 98936.46 | 209.27 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27607.98 | 27875.09 | 27723.36 | 85.99 |
| __bench_per_cpu_hash_map_update | Userspace | 44332.99 | 44468.24 | 44404.65 | 52.16 |
| __bench_read | Userspace | 4573.11 | 4585.48 | 4578.77 | 3.92 |
| __bench_uprobe | Userspace | 194.75 | 199.59 | 196.87 | 1.64 |
| __bench_uprobe_uretprobe | Userspace | 324.29 | 325.88 | 324.97 | 0.53 |
| __bench_uretprobe | Userspace | 266.53 | 271.64 | 268.17 | 1.35 |
| __bench_write | Userspace | 4270.02 | 4275.39 | 4272.27 | 1.85 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.95 | 182.33 | 158.72 | 8.26 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-25 22:19:00
- **Total duration:** 520.01 seconds

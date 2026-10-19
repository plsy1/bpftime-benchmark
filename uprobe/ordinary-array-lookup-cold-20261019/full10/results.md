# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-19 01:00:53*

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
| __bench_uprobe | 1227.72 | 195.66 | 6.27x |
| __bench_uretprobe | 1656.49 | 268.54 | 6.17x |
| __bench_uprobe_uretprobe | 1770.21 | 320.23 | 5.53x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3682.03 | 3692.17 | 3686.12 | 3.05 |
| __bench_array_map_delete | Userspace | 20080.16 | 20247.34 | 20164.85 | 55.01 |
| __bench_array_map_lookup | Kernel | 3805.95 | 3815.73 | 3810.12 | 3.32 |
| __bench_array_map_lookup | Userspace | 7458.15 | 7490.93 | 7467.29 | 11.87 |
| __bench_array_map_update | Kernel | 13061.35 | 13093.80 | 13078.13 | 11.64 |
| __bench_array_map_update | Userspace | 11486.38 | 11739.02 | 11547.33 | 68.70 |
| __bench_hash_map_delete | Kernel | 108711.24 | 108862.27 | 108781.90 | 49.86 |
| __bench_hash_map_delete | Userspace | 36385.97 | 36701.26 | 36584.08 | 94.90 |
| __bench_hash_map_lookup | Kernel | 28876.98 | 29037.46 | 28944.25 | 54.18 |
| __bench_hash_map_lookup | Userspace | 26839.27 | 27343.69 | 27068.54 | 157.73 |
| __bench_hash_map_update | Kernel | 94095.35 | 94240.94 | 94184.51 | 45.41 |
| __bench_hash_map_update | Userspace | 33033.73 | 38467.68 | 34423.42 | 1708.60 |
| __bench_per_cpu_array_map_delete | Kernel | 3681.76 | 3694.61 | 3687.42 | 4.40 |
| __bench_per_cpu_array_map_delete | Userspace | 20111.67 | 20278.91 | 20203.98 | 66.01 |
| __bench_per_cpu_array_map_lookup | Kernel | 4392.84 | 4409.37 | 4401.28 | 4.01 |
| __bench_per_cpu_array_map_lookup | Userspace | 10703.82 | 10816.00 | 10764.11 | 30.68 |
| __bench_per_cpu_array_map_update | Kernel | 16602.97 | 16655.06 | 16620.35 | 17.80 |
| __bench_per_cpu_array_map_update | Userspace | 14320.15 | 14600.09 | 14404.43 | 79.76 |
| __bench_per_cpu_hash_map_delete | Kernel | 150909.44 | 151003.88 | 150961.61 | 31.13 |
| __bench_per_cpu_hash_map_delete | Userspace | 98516.59 | 99236.36 | 98695.09 | 211.33 |
| __bench_per_cpu_hash_map_lookup | Kernel | 28820.11 | 29060.95 | 28953.92 | 67.84 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27584.25 | 27846.90 | 27722.89 | 83.63 |
| __bench_per_cpu_hash_map_update | Kernel | 67912.02 | 68005.18 | 67967.13 | 32.74 |
| __bench_per_cpu_hash_map_update | Userspace | 44287.23 | 44484.38 | 44389.35 | 50.50 |
| __bench_read | Kernel | 18003.25 | 18126.62 | 18102.02 | 38.27 |
| __bench_read | Userspace | 4573.69 | 4584.27 | 4578.35 | 3.50 |
| __bench_uprobe | Kernel | 1225.41 | 1234.22 | 1227.72 | 3.02 |
| __bench_uprobe | Userspace | 193.62 | 198.97 | 195.66 | 1.72 |
| __bench_uprobe_uretprobe | Kernel | 1749.37 | 1794.55 | 1770.21 | 11.47 |
| __bench_uprobe_uretprobe | Userspace | 317.90 | 322.68 | 320.23 | 1.55 |
| __bench_uretprobe | Kernel | 1638.64 | 1668.76 | 1656.49 | 9.33 |
| __bench_uretprobe | Userspace | 267.40 | 269.92 | 268.54 | 0.76 |
| __bench_write | Kernel | 17591.81 | 17816.42 | 17668.58 | 66.14 |
| __bench_write | Userspace | 4268.77 | 4276.93 | 4271.66 | 2.60 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.52 | 171.27 | 156.12 | 5.30 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-19 01:00:53
- **Total duration:** 1495.70 seconds

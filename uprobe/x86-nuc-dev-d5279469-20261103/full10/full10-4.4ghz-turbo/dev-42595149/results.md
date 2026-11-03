# BPFtime Uprobe Benchmark Results

*Generated on 2026-11-03 23:09:04*

## Environment

- **OS:** Linux 6.12.113+deb13-amd64
- **CPU:** 12th Gen Intel(R) Core(TM) i5-1240P (12 cores, 16 threads)
- **Memory:** 62.39 GB
- **Python:** 3.13.5

## Summary

This benchmark compares three different eBPF execution environments:
- **Kernel Uprobe**: Traditional kernel-based eBPF uprobes
- **Userspace Uprobe**: BPFtime's userspace implementation of uprobes
- **Embedded VM**: BPFtime's embedded eBPF VM

*Times shown in nanoseconds (ns) - lower is better*

### Core Uprobe Performance Summary

| Operation | Kernel Uprobe | Userspace Uprobe | Speedup |
|-----------|---------------|------------------|---------|
| __bench_uprobe | 1105.99 | 124.88 | 8.86x |
| __bench_uretprobe | 1294.39 | 186.31 | 6.95x |
| __bench_uprobe_uretprobe | 1339.73 | 216.96 | 6.18x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 1732.78 | 1752.15 | 1741.29 | 6.22 |
| __bench_array_map_delete | Userspace | 5792.09 | 9054.02 | 6251.26 | 936.44 |
| __bench_array_map_lookup | Kernel | 1802.62 | 1816.87 | 1809.13 | 4.87 |
| __bench_array_map_lookup | Userspace | 444.63 | 447.02 | 445.53 | 0.69 |
| __bench_array_map_update | Kernel | 7404.38 | 7614.27 | 7434.37 | 60.32 |
| __bench_array_map_update | Userspace | 3411.64 | 3423.04 | 3417.58 | 4.12 |
| __bench_hash_map_delete | Kernel | 34092.33 | 34303.92 | 34184.02 | 68.06 |
| __bench_hash_map_delete | Userspace | 15536.78 | 15715.99 | 15568.21 | 50.07 |
| __bench_hash_map_lookup | Kernel | 10586.33 | 10647.18 | 10614.02 | 19.16 |
| __bench_hash_map_lookup | Userspace | 5268.77 | 5501.97 | 5303.70 | 66.50 |
| __bench_hash_map_update | Kernel | 34908.77 | 35131.94 | 34995.57 | 75.20 |
| __bench_hash_map_update | Userspace | 9967.35 | 10218.99 | 10040.98 | 75.97 |
| __bench_per_cpu_array_map_delete | Kernel | 1731.02 | 1829.62 | 1749.33 | 27.27 |
| __bench_per_cpu_array_map_delete | Userspace | 5918.02 | 9041.13 | 6268.70 | 924.59 |
| __bench_per_cpu_array_map_lookup | Kernel | 1811.30 | 1826.70 | 1818.55 | 5.32 |
| __bench_per_cpu_array_map_lookup | Userspace | 1277.40 | 7646.24 | 1915.81 | 1910.14 |
| __bench_per_cpu_array_map_update | Kernel | 7508.19 | 7761.98 | 7548.93 | 71.69 |
| __bench_per_cpu_array_map_update | Userspace | 4932.83 | 11506.28 | 5686.51 | 1940.39 |
| __bench_per_cpu_hash_map_delete | Kernel | 111768.09 | 112662.38 | 112220.93 | 238.95 |
| __bench_per_cpu_hash_map_delete | Userspace | 25144.91 | 29160.25 | 25677.00 | 1170.66 |
| __bench_per_cpu_hash_map_lookup | Kernel | 10589.65 | 10766.62 | 10685.37 | 58.55 |
| __bench_per_cpu_hash_map_lookup | Userspace | 7137.06 | 11314.21 | 7610.92 | 1236.74 |
| __bench_per_cpu_hash_map_update | Kernel | 25537.36 | 25818.18 | 25613.22 | 92.68 |
| __bench_per_cpu_hash_map_update | Userspace | 11596.96 | 17110.22 | 12238.79 | 1625.21 |
| __bench_read | Kernel | 17946.45 | 19755.27 | 19171.60 | 590.36 |
| __bench_read | Userspace | 1504.33 | 1514.68 | 1507.93 | 2.53 |
| __bench_uprobe | Kernel | 1099.47 | 1111.08 | 1105.99 | 4.55 |
| __bench_uprobe | Userspace | 123.61 | 126.48 | 124.88 | 0.90 |
| __bench_uprobe_uretprobe | Kernel | 1333.13 | 1346.93 | 1339.73 | 4.87 |
| __bench_uprobe_uretprobe | Userspace | 215.35 | 218.36 | 216.96 | 0.95 |
| __bench_uretprobe | Kernel | 1288.74 | 1299.58 | 1294.39 | 4.10 |
| __bench_uretprobe | Userspace | 184.02 | 193.24 | 186.31 | 2.64 |
| __bench_write | Kernel | 19028.09 | 19320.74 | 19140.86 | 112.58 |
| __bench_write | Userspace | 1278.85 | 1288.63 | 1282.45 | 2.49 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 61.74 | 64.29 | 62.94 | 0.63 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-11-03 23:09:04
- **Total duration:** 606.99 seconds

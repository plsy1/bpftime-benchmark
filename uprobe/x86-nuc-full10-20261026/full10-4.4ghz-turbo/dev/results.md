# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-26 15:52:22*

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
| __bench_uprobe | 1107.91 | 123.99 | 8.94x |
| __bench_uretprobe | 1292.20 | 183.79 | 7.03x |
| __bench_uprobe_uretprobe | 1337.72 | 214.38 | 6.24x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 1732.27 | 1745.56 | 1740.21 | 4.27 |
| __bench_array_map_delete | Userspace | 5664.47 | 5686.93 | 5675.08 | 6.72 |
| __bench_array_map_lookup | Kernel | 1802.25 | 1816.63 | 1810.23 | 4.14 |
| __bench_array_map_lookup | Userspace | 444.97 | 447.52 | 446.42 | 0.74 |
| __bench_array_map_update | Kernel | 7410.40 | 7657.40 | 7444.34 | 71.28 |
| __bench_array_map_update | Userspace | 3559.87 | 3619.36 | 3578.16 | 18.98 |
| __bench_hash_map_delete | Kernel | 34216.19 | 34489.78 | 34359.27 | 96.78 |
| __bench_hash_map_delete | Userspace | 10137.73 | 10234.07 | 10160.87 | 26.40 |
| __bench_hash_map_lookup | Kernel | 10542.40 | 10586.97 | 10568.10 | 16.03 |
| __bench_hash_map_lookup | Userspace | 8591.20 | 8608.52 | 8599.54 | 5.42 |
| __bench_hash_map_update | Kernel | 34357.61 | 34620.89 | 34465.08 | 72.68 |
| __bench_hash_map_update | Userspace | 9966.62 | 10225.72 | 10017.72 | 77.25 |
| __bench_per_cpu_array_map_delete | Kernel | 1741.57 | 1755.48 | 1748.65 | 4.06 |
| __bench_per_cpu_array_map_delete | Userspace | 5670.64 | 5901.61 | 5701.84 | 67.09 |
| __bench_per_cpu_array_map_lookup | Kernel | 1810.39 | 2023.94 | 1839.26 | 61.70 |
| __bench_per_cpu_array_map_lookup | Userspace | 1277.80 | 1282.89 | 1279.41 | 1.43 |
| __bench_per_cpu_array_map_update | Kernel | 7414.92 | 7452.43 | 7428.15 | 9.96 |
| __bench_per_cpu_array_map_update | Userspace | 4733.53 | 4765.86 | 4740.25 | 9.30 |
| __bench_per_cpu_hash_map_delete | Kernel | 112053.93 | 112290.49 | 112214.69 | 72.75 |
| __bench_per_cpu_hash_map_delete | Userspace | 30366.45 | 30658.07 | 30511.57 | 78.09 |
| __bench_per_cpu_hash_map_lookup | Kernel | 10486.48 | 10751.14 | 10612.98 | 64.73 |
| __bench_per_cpu_hash_map_lookup | Userspace | 6971.14 | 7012.94 | 6993.44 | 13.63 |
| __bench_per_cpu_hash_map_update | Kernel | 25652.94 | 26032.79 | 25814.42 | 137.69 |
| __bench_per_cpu_hash_map_update | Userspace | 12568.76 | 12812.99 | 12629.81 | 64.48 |
| __bench_read | Kernel | 17940.74 | 19564.74 | 18937.77 | 651.95 |
| __bench_read | Userspace | 1504.18 | 1514.13 | 1507.45 | 3.28 |
| __bench_uprobe | Kernel | 1100.78 | 1116.53 | 1107.91 | 4.09 |
| __bench_uprobe | Userspace | 122.57 | 125.96 | 123.99 | 0.97 |
| __bench_uprobe_uretprobe | Kernel | 1330.17 | 1342.76 | 1337.72 | 3.15 |
| __bench_uprobe_uretprobe | Userspace | 212.63 | 216.12 | 214.38 | 0.92 |
| __bench_uretprobe | Kernel | 1284.51 | 1296.43 | 1292.20 | 3.56 |
| __bench_uretprobe | Userspace | 182.31 | 186.62 | 183.79 | 1.20 |
| __bench_write | Kernel | 19067.66 | 19358.85 | 19130.42 | 79.05 |
| __bench_write | Userspace | 1280.70 | 1298.62 | 1285.24 | 5.62 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 62.04 | 64.02 | 63.09 | 0.61 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-26 15:52:22
- **Total duration:** 614.44 seconds

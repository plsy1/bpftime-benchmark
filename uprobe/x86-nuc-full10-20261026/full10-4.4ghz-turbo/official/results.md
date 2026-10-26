# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-26 16:08:17*

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
| __bench_uprobe | 1102.96 | 122.92 | 8.97x |
| __bench_uretprobe | 1290.28 | 182.40 | 7.07x |
| __bench_uprobe_uretprobe | 1337.22 | 211.95 | 6.31x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 1731.72 | 1743.72 | 1738.93 | 3.90 |
| __bench_array_map_delete | Userspace | 5875.53 | 6008.16 | 5952.06 | 38.08 |
| __bench_array_map_lookup | Kernel | 1805.00 | 1821.33 | 1811.67 | 5.90 |
| __bench_array_map_lookup | Userspace | 5765.76 | 5786.22 | 5780.84 | 5.90 |
| __bench_array_map_update | Kernel | 7398.81 | 7503.48 | 7452.75 | 40.21 |
| __bench_array_map_update | Userspace | 8719.44 | 9160.65 | 8938.73 | 121.49 |
| __bench_hash_map_delete | Kernel | 34181.54 | 34491.13 | 34354.83 | 88.88 |
| __bench_hash_map_delete | Userspace | 17586.20 | 19862.38 | 18334.05 | 673.80 |
| __bench_hash_map_lookup | Kernel | 9861.05 | 10030.44 | 9940.37 | 46.28 |
| __bench_hash_map_lookup | Userspace | 20137.68 | 21417.43 | 20652.44 | 409.18 |
| __bench_hash_map_update | Kernel | 34885.74 | 35385.74 | 35087.74 | 162.29 |
| __bench_hash_map_update | Userspace | 22614.62 | 24392.81 | 23478.27 | 595.65 |
| __bench_per_cpu_array_map_delete | Kernel | 1738.87 | 1779.14 | 1751.61 | 10.78 |
| __bench_per_cpu_array_map_delete | Userspace | 5878.48 | 6036.73 | 5957.63 | 41.29 |
| __bench_per_cpu_array_map_lookup | Kernel | 1808.21 | 1822.79 | 1816.14 | 4.14 |
| __bench_per_cpu_array_map_lookup | Userspace | 9825.98 | 10275.02 | 9975.42 | 119.34 |
| __bench_per_cpu_array_map_update | Kernel | 7441.85 | 7687.68 | 7480.16 | 69.94 |
| __bench_per_cpu_array_map_update | Userspace | 18066.04 | 18209.40 | 18150.61 | 39.21 |
| __bench_per_cpu_hash_map_delete | Kernel | 111741.26 | 112102.51 | 111950.91 | 107.75 |
| __bench_per_cpu_hash_map_delete | Userspace | 64426.29 | 64585.12 | 64498.92 | 56.65 |
| __bench_per_cpu_hash_map_lookup | Kernel | 10603.83 | 10680.81 | 10631.22 | 23.60 |
| __bench_per_cpu_hash_map_lookup | Userspace | 45021.46 | 45360.70 | 45146.43 | 107.51 |
| __bench_per_cpu_hash_map_update | Kernel | 25288.09 | 25403.73 | 25326.51 | 30.62 |
| __bench_per_cpu_hash_map_update | Userspace | 70500.43 | 71071.92 | 70641.88 | 158.90 |
| __bench_read | Kernel | 17958.54 | 19525.92 | 18956.95 | 604.54 |
| __bench_read | Userspace | 1653.83 | 1729.84 | 1698.57 | 21.53 |
| __bench_uprobe | Kernel | 1099.21 | 1109.13 | 1102.96 | 3.68 |
| __bench_uprobe | Userspace | 121.97 | 123.92 | 122.92 | 0.51 |
| __bench_uprobe_uretprobe | Kernel | 1329.47 | 1344.76 | 1337.22 | 5.65 |
| __bench_uprobe_uretprobe | Userspace | 209.86 | 214.29 | 211.95 | 1.18 |
| __bench_uretprobe | Kernel | 1284.19 | 1297.64 | 1290.28 | 4.63 |
| __bench_uretprobe | Userspace | 181.52 | 184.06 | 182.40 | 0.71 |
| __bench_write | Kernel | 19032.26 | 19737.25 | 19157.87 | 204.04 |
| __bench_write | Userspace | 1505.13 | 1509.79 | 1506.46 | 1.28 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 62.66 | 64.51 | 63.61 | 0.56 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-26 16:08:17
- **Total duration:** 943.24 seconds

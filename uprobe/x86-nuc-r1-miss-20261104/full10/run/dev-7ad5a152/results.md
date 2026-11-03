# BPFtime Uprobe Benchmark Results

*Generated on 2026-11-04 01:07:36*

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
| __bench_uprobe | 1116.09 | 123.67 | 9.02x |
| __bench_uretprobe | 1296.45 | 183.11 | 7.08x |
| __bench_uprobe_uretprobe | 1341.33 | 214.13 | 6.26x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 1733.63 | 1751.26 | 1743.40 | 4.92 |
| __bench_array_map_delete | Userspace | 5680.55 | 5717.17 | 5702.45 | 10.80 |
| __bench_array_map_lookup | Kernel | 1812.63 | 1826.06 | 1818.53 | 4.55 |
| __bench_array_map_lookup | Userspace | 443.69 | 463.37 | 447.12 | 5.48 |
| __bench_array_map_update | Kernel | 7405.24 | 7426.65 | 7414.02 | 5.95 |
| __bench_array_map_update | Userspace | 3410.22 | 6585.02 | 3768.63 | 940.79 |
| __bench_hash_map_delete | Kernel | 34202.50 | 34372.12 | 34306.17 | 45.92 |
| __bench_hash_map_delete | Userspace | 14290.31 | 19660.22 | 15041.72 | 1547.99 |
| __bench_hash_map_lookup | Kernel | 10535.69 | 10773.60 | 10602.35 | 62.83 |
| __bench_hash_map_lookup | Userspace | 4672.91 | 10857.56 | 5324.70 | 1844.36 |
| __bench_hash_map_update | Kernel | 34425.45 | 34947.73 | 34605.54 | 166.61 |
| __bench_hash_map_update | Userspace | 10385.17 | 14226.44 | 10920.36 | 1106.64 |
| __bench_per_cpu_array_map_delete | Kernel | 1737.68 | 1763.51 | 1745.41 | 7.43 |
| __bench_per_cpu_array_map_delete | Userspace | 5671.36 | 5897.01 | 5715.24 | 61.67 |
| __bench_per_cpu_array_map_lookup | Kernel | 1813.60 | 1828.88 | 1821.08 | 4.17 |
| __bench_per_cpu_array_map_lookup | Userspace | 1276.66 | 1284.77 | 1278.14 | 2.29 |
| __bench_per_cpu_array_map_update | Kernel | 7537.76 | 7740.18 | 7569.09 | 58.90 |
| __bench_per_cpu_array_map_update | Userspace | 4797.04 | 8166.51 | 5155.96 | 1003.71 |
| __bench_per_cpu_hash_map_delete | Kernel | 112267.21 | 112956.55 | 112528.51 | 197.74 |
| __bench_per_cpu_hash_map_delete | Userspace | 25548.49 | 27560.67 | 26023.02 | 559.58 |
| __bench_per_cpu_hash_map_lookup | Kernel | 11137.62 | 11326.62 | 11202.59 | 55.41 |
| __bench_per_cpu_hash_map_lookup | Userspace | 7064.84 | 12253.72 | 7656.57 | 1535.05 |
| __bench_per_cpu_hash_map_update | Kernel | 25439.01 | 25781.48 | 25559.46 | 96.47 |
| __bench_per_cpu_hash_map_update | Userspace | 11880.45 | 16753.83 | 12460.59 | 1436.68 |
| __bench_read | Kernel | 17938.59 | 19335.34 | 18933.28 | 601.86 |
| __bench_read | Userspace | 1505.94 | 1516.42 | 1507.65 | 3.00 |
| __bench_uprobe | Kernel | 1101.53 | 1181.88 | 1116.09 | 22.26 |
| __bench_uprobe | Userspace | 122.28 | 124.79 | 123.67 | 0.72 |
| __bench_uprobe_uretprobe | Kernel | 1333.83 | 1352.61 | 1341.33 | 5.02 |
| __bench_uprobe_uretprobe | Userspace | 213.07 | 215.42 | 214.13 | 0.76 |
| __bench_uretprobe | Kernel | 1285.45 | 1311.57 | 1296.45 | 6.54 |
| __bench_uretprobe | Userspace | 181.39 | 184.57 | 183.11 | 0.83 |
| __bench_write | Kernel | 19037.98 | 22748.54 | 19466.39 | 1097.12 |
| __bench_write | Userspace | 1280.33 | 1288.69 | 1282.75 | 2.25 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 62.30 | 64.31 | 63.10 | 0.62 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-11-04 01:07:36
- **Total duration:** 606.94 seconds

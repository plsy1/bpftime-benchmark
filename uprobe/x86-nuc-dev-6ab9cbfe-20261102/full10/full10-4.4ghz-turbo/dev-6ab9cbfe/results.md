# BPFtime Uprobe Benchmark Results

*Generated on 2026-11-02 05:30:19*

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
| __bench_uprobe | 1113.52 | 122.81 | 9.07x |
| __bench_uretprobe | 1296.90 | 183.80 | 7.06x |
| __bench_uprobe_uretprobe | 1339.80 | 212.96 | 6.29x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 1735.44 | 1755.72 | 1747.16 | 5.30 |
| __bench_array_map_delete | Userspace | 5473.17 | 5547.51 | 5505.91 | 20.95 |
| __bench_array_map_lookup | Kernel | 1810.79 | 1827.00 | 1818.66 | 5.00 |
| __bench_array_map_lookup | Userspace | 444.68 | 447.52 | 445.40 | 0.81 |
| __bench_array_map_update | Kernel | 7404.54 | 7443.64 | 7420.22 | 11.19 |
| __bench_array_map_update | Userspace | 3341.62 | 3475.66 | 3361.67 | 38.74 |
| __bench_hash_map_delete | Kernel | 34328.58 | 34677.45 | 34477.18 | 121.48 |
| __bench_hash_map_delete | Userspace | 13619.60 | 13775.31 | 13706.23 | 53.22 |
| __bench_hash_map_lookup | Kernel | 10014.03 | 10300.65 | 10071.59 | 83.76 |
| __bench_hash_map_lookup | Userspace | 5265.49 | 5297.55 | 5279.97 | 10.07 |
| __bench_hash_map_update | Kernel | 35091.43 | 35429.21 | 35219.60 | 112.46 |
| __bench_hash_map_update | Userspace | 11867.47 | 12295.48 | 11981.96 | 115.65 |
| __bench_per_cpu_array_map_delete | Kernel | 1736.69 | 1760.75 | 1747.12 | 7.28 |
| __bench_per_cpu_array_map_delete | Userspace | 5478.81 | 5747.94 | 5553.46 | 70.50 |
| __bench_per_cpu_array_map_lookup | Kernel | 1818.23 | 1829.82 | 1825.59 | 3.76 |
| __bench_per_cpu_array_map_lookup | Userspace | 1277.39 | 1284.72 | 1279.78 | 1.97 |
| __bench_per_cpu_array_map_update | Kernel | 7423.36 | 7435.73 | 7430.38 | 4.64 |
| __bench_per_cpu_array_map_update | Userspace | 4723.73 | 4751.31 | 4732.11 | 7.70 |
| __bench_per_cpu_hash_map_delete | Kernel | 111244.70 | 111674.28 | 111525.97 | 120.50 |
| __bench_per_cpu_hash_map_delete | Userspace | 31674.77 | 32127.32 | 31871.44 | 119.32 |
| __bench_per_cpu_hash_map_lookup | Kernel | 10679.28 | 10883.28 | 10758.51 | 56.89 |
| __bench_per_cpu_hash_map_lookup | Userspace | 10226.74 | 10286.98 | 10242.78 | 16.73 |
| __bench_per_cpu_hash_map_update | Kernel | 25507.04 | 25844.93 | 25637.77 | 104.93 |
| __bench_per_cpu_hash_map_update | Userspace | 15991.72 | 16269.51 | 16138.61 | 95.86 |
| __bench_read | Kernel | 17926.18 | 19386.46 | 18924.40 | 639.20 |
| __bench_read | Userspace | 1504.06 | 1548.53 | 1512.48 | 13.01 |
| __bench_uprobe | Kernel | 1103.08 | 1158.40 | 1113.52 | 15.34 |
| __bench_uprobe | Userspace | 122.01 | 124.03 | 122.81 | 0.72 |
| __bench_uprobe_uretprobe | Kernel | 1331.89 | 1345.31 | 1339.80 | 4.32 |
| __bench_uprobe_uretprobe | Userspace | 209.81 | 215.34 | 212.96 | 1.62 |
| __bench_uretprobe | Kernel | 1291.87 | 1299.41 | 1296.90 | 2.31 |
| __bench_uretprobe | Userspace | 183.34 | 184.22 | 183.80 | 0.30 |
| __bench_write | Kernel | 19073.99 | 19171.09 | 19117.50 | 28.90 |
| __bench_write | Userspace | 1278.34 | 1287.64 | 1280.62 | 2.45 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 62.91 | 66.94 | 63.82 | 1.15 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-11-02 05:30:19
- **Total duration:** 629.81 seconds

# BPFtime Uprobe Benchmark Results

*Generated on 2026-11-03 01:39:15*

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
| __bench_uprobe | 1105.81 | 123.16 | 8.98x |
| __bench_uretprobe | 1298.17 | 184.86 | 7.02x |
| __bench_uprobe_uretprobe | 1346.53 | 212.98 | 6.32x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 1737.38 | 1748.24 | 1742.81 | 5.43 |
| __bench_array_map_delete | Userspace | 5526.94 | 5540.12 | 5533.53 | 6.59 |
| __bench_array_map_lookup | Kernel | 1810.23 | 1828.42 | 1819.32 | 9.09 |
| __bench_array_map_lookup | Userspace | 444.66 | 446.93 | 445.80 | 1.14 |
| __bench_array_map_update | Kernel | 7415.32 | 7495.35 | 7455.34 | 40.02 |
| __bench_array_map_update | Userspace | 3341.02 | 3342.14 | 3341.58 | 0.56 |
| __bench_hash_map_delete | Kernel | 34674.75 | 34687.85 | 34681.30 | 6.55 |
| __bench_hash_map_delete | Userspace | 13594.71 | 13673.07 | 13633.89 | 39.18 |
| __bench_hash_map_lookup | Kernel | 10513.53 | 10678.49 | 10596.01 | 82.48 |
| __bench_hash_map_lookup | Userspace | 5291.22 | 5518.29 | 5404.76 | 113.53 |
| __bench_hash_map_update | Kernel | 34793.73 | 35028.97 | 34911.35 | 117.62 |
| __bench_hash_map_update | Userspace | 11953.79 | 12005.29 | 11979.54 | 25.75 |
| __bench_per_cpu_array_map_delete | Kernel | 1741.35 | 1745.30 | 1743.32 | 1.98 |
| __bench_per_cpu_array_map_delete | Userspace | 5508.44 | 5536.38 | 5522.41 | 13.97 |
| __bench_per_cpu_array_map_lookup | Kernel | 1816.88 | 1821.70 | 1819.29 | 2.41 |
| __bench_per_cpu_array_map_lookup | Userspace | 1277.97 | 1278.00 | 1277.98 | 0.01 |
| __bench_per_cpu_array_map_update | Kernel | 7444.26 | 7444.61 | 7444.44 | 0.17 |
| __bench_per_cpu_array_map_update | Userspace | 4722.73 | 4731.95 | 4727.34 | 4.61 |
| __bench_per_cpu_hash_map_delete | Kernel | 111479.89 | 111613.35 | 111546.62 | 66.73 |
| __bench_per_cpu_hash_map_delete | Userspace | 31679.04 | 31701.09 | 31690.06 | 11.03 |
| __bench_per_cpu_hash_map_lookup | Kernel | 10742.55 | 10752.39 | 10747.47 | 4.92 |
| __bench_per_cpu_hash_map_lookup | Userspace | 10237.90 | 10243.73 | 10240.81 | 2.91 |
| __bench_per_cpu_hash_map_update | Kernel | 25328.35 | 25349.54 | 25338.94 | 10.60 |
| __bench_per_cpu_hash_map_update | Userspace | 15949.47 | 16332.69 | 16141.08 | 191.61 |
| __bench_read | Kernel | 17920.98 | 19313.42 | 18617.20 | 696.22 |
| __bench_read | Userspace | 1506.63 | 1506.69 | 1506.66 | 0.03 |
| __bench_uprobe | Kernel | 1101.52 | 1110.10 | 1105.81 | 4.29 |
| __bench_uprobe | Userspace | 122.31 | 124.01 | 123.16 | 0.85 |
| __bench_uprobe_uretprobe | Kernel | 1343.24 | 1349.82 | 1346.53 | 3.29 |
| __bench_uprobe_uretprobe | Userspace | 212.27 | 213.70 | 212.98 | 0.71 |
| __bench_uretprobe | Kernel | 1296.48 | 1299.85 | 1298.17 | 1.69 |
| __bench_uretprobe | Userspace | 184.22 | 185.50 | 184.86 | 0.64 |
| __bench_write | Kernel | 19145.73 | 22792.65 | 20969.19 | 1823.46 |
| __bench_write | Userspace | 1279.69 | 1279.77 | 1279.73 | 0.04 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 63.15 | 63.23 | 63.19 | 0.04 |

## Benchmark Metadata

- **Number of runs:** 2
- **Timestamp:** 2026-11-03 01:39:15
- **Total duration:** 136.37 seconds

# BPFtime Uprobe Benchmark Results

*Generated on 2026-11-02 05:40:47*

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
| __bench_uprobe | 1105.34 | 123.60 | 8.94x |
| __bench_uretprobe | 1294.05 | 182.95 | 7.07x |
| __bench_uprobe_uretprobe | 1338.71 | 214.33 | 6.25x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 1730.92 | 1763.90 | 1742.63 | 8.53 |
| __bench_array_map_delete | Userspace | 5670.60 | 5722.67 | 5683.34 | 14.15 |
| __bench_array_map_lookup | Kernel | 1808.66 | 1870.61 | 1823.88 | 16.73 |
| __bench_array_map_lookup | Userspace | 445.14 | 449.56 | 446.75 | 1.17 |
| __bench_array_map_update | Kernel | 7414.74 | 7454.81 | 7426.11 | 12.05 |
| __bench_array_map_update | Userspace | 3342.60 | 3479.75 | 3367.23 | 41.28 |
| __bench_hash_map_delete | Kernel | 34337.48 | 34583.63 | 34443.11 | 81.68 |
| __bench_hash_map_delete | Userspace | 10137.72 | 10261.86 | 10163.78 | 34.84 |
| __bench_hash_map_lookup | Kernel | 10267.02 | 10544.54 | 10354.47 | 74.60 |
| __bench_hash_map_lookup | Userspace | 8600.84 | 8645.88 | 8610.82 | 12.36 |
| __bench_hash_map_update | Kernel | 34421.56 | 34874.33 | 34591.81 | 135.73 |
| __bench_hash_map_update | Userspace | 9969.81 | 10246.33 | 10046.08 | 71.88 |
| __bench_per_cpu_array_map_delete | Kernel | 1737.45 | 1775.86 | 1749.55 | 11.37 |
| __bench_per_cpu_array_map_delete | Userspace | 5683.71 | 5701.97 | 5691.75 | 5.85 |
| __bench_per_cpu_array_map_lookup | Kernel | 1809.11 | 1821.21 | 1815.79 | 4.30 |
| __bench_per_cpu_array_map_lookup | Userspace | 1278.50 | 1285.23 | 1280.22 | 1.99 |
| __bench_per_cpu_array_map_update | Kernel | 7424.05 | 7530.99 | 7470.78 | 40.18 |
| __bench_per_cpu_array_map_update | Userspace | 4733.44 | 4933.12 | 4761.28 | 58.39 |
| __bench_per_cpu_hash_map_delete | Kernel | 113305.02 | 113598.69 | 113439.02 | 102.27 |
| __bench_per_cpu_hash_map_delete | Userspace | 30365.47 | 30621.42 | 30472.06 | 71.92 |
| __bench_per_cpu_hash_map_lookup | Kernel | 10868.45 | 11020.25 | 10936.20 | 43.55 |
| __bench_per_cpu_hash_map_lookup | Userspace | 6963.06 | 7022.41 | 6984.12 | 17.15 |
| __bench_per_cpu_hash_map_update | Kernel | 25632.31 | 25864.17 | 25776.06 | 60.63 |
| __bench_per_cpu_hash_map_update | Userspace | 12590.29 | 12842.18 | 12653.92 | 67.53 |
| __bench_read | Kernel | 17945.20 | 19367.42 | 19057.31 | 553.68 |
| __bench_read | Userspace | 1504.21 | 1510.02 | 1506.19 | 1.56 |
| __bench_uprobe | Kernel | 1100.29 | 1111.39 | 1105.34 | 4.15 |
| __bench_uprobe | Userspace | 122.31 | 124.68 | 123.60 | 0.62 |
| __bench_uprobe_uretprobe | Kernel | 1330.43 | 1358.11 | 1338.71 | 7.84 |
| __bench_uprobe_uretprobe | Userspace | 213.26 | 216.72 | 214.33 | 1.06 |
| __bench_uretprobe | Kernel | 1285.55 | 1303.82 | 1294.05 | 6.58 |
| __bench_uretprobe | Userspace | 182.21 | 183.55 | 182.95 | 0.39 |
| __bench_write | Kernel | 19124.74 | 19228.96 | 19154.55 | 28.25 |
| __bench_write | Userspace | 1280.50 | 1286.65 | 1282.59 | 1.84 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 62.01 | 64.19 | 63.28 | 0.65 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-11-02 05:40:47
- **Total duration:** 616.87 seconds

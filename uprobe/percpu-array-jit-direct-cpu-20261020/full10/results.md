# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-20 20:58:30*

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
| __bench_uprobe | 1229.44 | 198.41 | 6.20x |
| __bench_uretprobe | 1644.66 | 271.70 | 6.05x |
| __bench_uprobe_uretprobe | 1758.27 | 325.87 | 5.40x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3515.38 | 3534.66 | 3522.49 | 6.34 |
| __bench_array_map_delete | Userspace | 20640.01 | 20886.68 | 20738.46 | 61.11 |
| __bench_array_map_lookup | Kernel | 3785.18 | 3797.66 | 3791.28 | 4.48 |
| __bench_array_map_lookup | Userspace | 1172.68 | 1191.57 | 1177.24 | 4.98 |
| __bench_array_map_update | Kernel | 13001.19 | 13037.99 | 13018.17 | 12.03 |
| __bench_array_map_update | Userspace | 11511.98 | 12047.09 | 11618.81 | 156.65 |
| __bench_hash_map_delete | Kernel | 107991.54 | 108117.72 | 108053.25 | 35.01 |
| __bench_hash_map_delete | Userspace | 36919.63 | 37463.11 | 37058.96 | 167.60 |
| __bench_hash_map_lookup | Kernel | 28330.71 | 28520.08 | 28403.34 | 55.91 |
| __bench_hash_map_lookup | Userspace | 26820.80 | 27118.46 | 26965.62 | 113.48 |
| __bench_hash_map_update | Kernel | 94381.22 | 94525.82 | 94451.01 | 35.97 |
| __bench_hash_map_update | Userspace | 33016.56 | 36000.20 | 34542.45 | 1370.62 |
| __bench_per_cpu_array_map_delete | Kernel | 3681.65 | 3698.93 | 3689.22 | 6.09 |
| __bench_per_cpu_array_map_delete | Userspace | 20693.13 | 20819.78 | 20771.82 | 31.32 |
| __bench_per_cpu_array_map_lookup | Kernel | 4396.77 | 4413.07 | 4402.41 | 5.48 |
| __bench_per_cpu_array_map_lookup | Userspace | 4102.32 | 4126.47 | 4110.82 | 8.13 |
| __bench_per_cpu_array_map_update | Kernel | 16570.91 | 16631.70 | 16597.23 | 16.56 |
| __bench_per_cpu_array_map_update | Userspace | 14448.56 | 14826.75 | 14552.42 | 99.86 |
| __bench_per_cpu_hash_map_delete | Kernel | 149808.74 | 149895.59 | 149859.08 | 26.78 |
| __bench_per_cpu_hash_map_delete | Userspace | 99578.96 | 100293.31 | 99885.88 | 265.62 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29452.61 | 29607.33 | 29528.75 | 44.62 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27619.37 | 27787.58 | 27702.09 | 51.44 |
| __bench_per_cpu_hash_map_update | Kernel | 67884.65 | 68008.70 | 67952.03 | 40.17 |
| __bench_per_cpu_hash_map_update | Userspace | 44367.43 | 44539.58 | 44446.38 | 62.00 |
| __bench_read | Kernel | 18061.12 | 19461.20 | 18246.09 | 406.42 |
| __bench_read | Userspace | 4566.90 | 4573.35 | 4570.02 | 2.10 |
| __bench_uprobe | Kernel | 1224.90 | 1236.39 | 1229.44 | 4.17 |
| __bench_uprobe | Userspace | 197.11 | 200.93 | 198.41 | 1.16 |
| __bench_uprobe_uretprobe | Kernel | 1750.11 | 1767.17 | 1758.27 | 5.50 |
| __bench_uprobe_uretprobe | Userspace | 324.80 | 328.39 | 325.87 | 1.07 |
| __bench_uretprobe | Kernel | 1638.30 | 1658.17 | 1644.66 | 6.00 |
| __bench_uretprobe | Userspace | 270.59 | 274.34 | 271.70 | 1.16 |
| __bench_write | Kernel | 17662.52 | 17790.80 | 17713.68 | 41.83 |
| __bench_write | Userspace | 4273.06 | 4277.34 | 4274.46 | 1.24 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.68 | 179.27 | 158.34 | 7.59 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-20 20:58:30
- **Total duration:** 1487.68 seconds

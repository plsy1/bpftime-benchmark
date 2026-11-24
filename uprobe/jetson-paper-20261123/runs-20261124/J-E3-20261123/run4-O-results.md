# BPFtime Uprobe Benchmark Results

*Generated on 2026-11-23 21:29:14*

## Environment

- **OS:** Linux 6.8.12-1021-tegra-btf
- **CPU:** Unknown (0 cores, 6 threads)
- **Memory:** 7.36 GB
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
| __bench_uprobe | 1248.24 | 197.01 | 6.34x |
| __bench_uretprobe | 1666.73 | 268.09 | 6.22x |
| __bench_uprobe_uretprobe | 1780.69 | 322.63 | 5.52x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3725.58 | 3749.97 | 3743.22 | 6.13 |
| __bench_array_map_delete | Userspace | 20228.12 | 20417.52 | 20293.98 | 62.66 |
| __bench_array_map_lookup | Kernel | 3817.50 | 3844.49 | 3823.77 | 7.83 |
| __bench_array_map_lookup | Userspace | 19944.38 | 20083.49 | 20025.78 | 46.16 |
| __bench_array_map_update | Kernel | 13112.05 | 13152.31 | 13130.24 | 10.59 |
| __bench_array_map_update | Userspace | 25407.42 | 25885.06 | 25510.52 | 131.68 |
| __bench_hash_map_delete | Kernel | 108831.47 | 109622.79 | 109019.01 | 265.79 |
| __bench_hash_map_delete | Userspace | 47225.68 | 49063.07 | 47671.10 | 512.94 |
| __bench_hash_map_lookup | Kernel | 28979.98 | 29467.90 | 29136.15 | 139.62 |
| __bench_hash_map_lookup | Userspace | 51646.24 | 52655.14 | 52144.07 | 398.17 |
| __bench_hash_map_update | Kernel | 95854.16 | 95945.63 | 95907.63 | 27.67 |
| __bench_hash_map_update | Userspace | 61427.68 | 62052.69 | 61760.24 | 208.17 |
| __bench_per_cpu_array_map_delete | Kernel | 3759.09 | 3791.07 | 3783.64 | 8.65 |
| __bench_per_cpu_array_map_delete | Userspace | 20249.20 | 20386.50 | 20317.39 | 46.91 |
| __bench_per_cpu_array_map_lookup | Kernel | 4503.85 | 4525.86 | 4521.99 | 6.15 |
| __bench_per_cpu_array_map_lookup | Userspace | 28452.54 | 28873.11 | 28671.35 | 126.35 |
| __bench_per_cpu_array_map_update | Kernel | 16649.83 | 16671.94 | 16659.16 | 7.01 |
| __bench_per_cpu_array_map_update | Userspace | 71973.50 | 72539.49 | 72178.86 | 165.16 |
| __bench_per_cpu_hash_map_delete | Kernel | 151111.20 | 151211.34 | 151169.59 | 31.95 |
| __bench_per_cpu_hash_map_delete | Userspace | 256173.91 | 256918.21 | 256687.21 | 195.60 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29545.49 | 29746.37 | 29663.35 | 53.18 |
| __bench_per_cpu_hash_map_lookup | Userspace | 127772.70 | 128248.51 | 127989.64 | 150.60 |
| __bench_per_cpu_hash_map_update | Kernel | 68024.13 | 68099.80 | 68052.31 | 26.18 |
| __bench_per_cpu_hash_map_update | Userspace | 207164.28 | 210304.77 | 209470.77 | 841.61 |
| __bench_read | Kernel | 17561.77 | 17643.24 | 17603.91 | 26.27 |
| __bench_read | Userspace | 4576.75 | 4582.97 | 4580.58 | 1.91 |
| __bench_uprobe | Kernel | 1239.17 | 1265.74 | 1248.24 | 8.85 |
| __bench_uprobe | Userspace | 195.29 | 201.71 | 197.01 | 1.83 |
| __bench_uprobe_uretprobe | Kernel | 1768.05 | 1793.63 | 1780.69 | 9.15 |
| __bench_uprobe_uretprobe | Userspace | 318.79 | 332.36 | 322.63 | 4.81 |
| __bench_uretprobe | Kernel | 1657.87 | 1689.42 | 1666.73 | 9.78 |
| __bench_uretprobe | Userspace | 266.71 | 271.11 | 268.09 | 1.25 |
| __bench_write | Kernel | 17730.50 | 17854.64 | 17789.69 | 33.91 |
| __bench_write | Userspace | 4269.59 | 4278.08 | 4274.01 | 2.86 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 153.50 | 161.56 | 156.67 | 2.54 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-11-23 21:29:14
- **Total duration:** 2253.55 seconds

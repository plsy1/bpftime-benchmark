# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-26 13:33:22*

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
| __bench_uprobe | 1244.58 | 201.32 | 6.18x |
| __bench_uretprobe | 1673.19 | 273.42 | 6.12x |
| __bench_uprobe_uretprobe | 1782.31 | 328.24 | 5.43x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3745.57 | 3763.79 | 3752.93 | 5.38 |
| __bench_array_map_delete | Userspace | 21028.42 | 21387.36 | 21236.27 | 120.40 |
| __bench_array_map_lookup | Kernel | 3817.44 | 3834.02 | 3824.61 | 5.00 |
| __bench_array_map_lookup | Userspace | 1174.45 | 1176.72 | 1175.35 | 0.61 |
| __bench_array_map_update | Kernel | 13107.84 | 13141.77 | 13125.63 | 8.98 |
| __bench_array_map_update | Userspace | 11494.19 | 11642.12 | 11536.49 | 44.64 |
| __bench_hash_map_delete | Kernel | 108167.50 | 108265.86 | 108211.45 | 26.63 |
| __bench_hash_map_delete | Userspace | 27255.83 | 29710.14 | 28194.99 | 726.68 |
| __bench_hash_map_lookup | Kernel | 28375.38 | 28623.45 | 28489.87 | 83.67 |
| __bench_hash_map_lookup | Userspace | 22928.80 | 24089.84 | 23315.35 | 337.16 |
| __bench_hash_map_update | Kernel | 95130.80 | 95313.28 | 95214.27 | 52.16 |
| __bench_hash_map_update | Userspace | 26544.39 | 28597.75 | 27831.32 | 687.00 |
| __bench_per_cpu_array_map_delete | Kernel | 3568.12 | 3584.08 | 3574.71 | 4.70 |
| __bench_per_cpu_array_map_delete | Userspace | 21059.36 | 21395.44 | 21268.61 | 112.39 |
| __bench_per_cpu_array_map_lookup | Kernel | 4322.02 | 4333.59 | 4325.69 | 3.32 |
| __bench_per_cpu_array_map_lookup | Userspace | 4106.59 | 4127.06 | 4116.71 | 7.32 |
| __bench_per_cpu_array_map_update | Kernel | 16648.48 | 16704.04 | 16676.43 | 17.61 |
| __bench_per_cpu_array_map_update | Userspace | 14397.98 | 14555.80 | 14473.86 | 46.58 |
| __bench_per_cpu_hash_map_delete | Kernel | 149143.06 | 149363.62 | 149259.52 | 68.27 |
| __bench_per_cpu_hash_map_delete | Userspace | 99484.08 | 99841.12 | 99706.12 | 105.03 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29545.04 | 29706.68 | 29645.22 | 45.94 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27626.74 | 27907.10 | 27821.88 | 88.10 |
| __bench_per_cpu_hash_map_update | Kernel | 67614.40 | 67904.12 | 67724.37 | 103.41 |
| __bench_per_cpu_hash_map_update | Userspace | 44379.57 | 44756.61 | 44497.11 | 114.38 |
| __bench_read | Kernel | 17527.97 | 17608.84 | 17572.48 | 22.32 |
| __bench_read | Userspace | 4568.73 | 4575.74 | 4570.74 | 1.83 |
| __bench_uprobe | Kernel | 1238.57 | 1259.20 | 1244.58 | 7.72 |
| __bench_uprobe | Userspace | 199.19 | 202.36 | 201.32 | 1.00 |
| __bench_uprobe_uretprobe | Kernel | 1770.41 | 1807.53 | 1782.31 | 11.18 |
| __bench_uprobe_uretprobe | Userspace | 326.60 | 332.37 | 328.24 | 1.53 |
| __bench_uretprobe | Kernel | 1660.81 | 1689.17 | 1673.19 | 9.10 |
| __bench_uretprobe | Userspace | 271.33 | 277.46 | 273.42 | 1.82 |
| __bench_write | Kernel | 17734.94 | 18061.28 | 17857.97 | 78.07 |
| __bench_write | Userspace | 4272.50 | 4343.44 | 4286.93 | 24.37 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 153.76 | 177.93 | 158.49 | 6.82 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-26 13:33:22
- **Total duration:** 1395.31 seconds

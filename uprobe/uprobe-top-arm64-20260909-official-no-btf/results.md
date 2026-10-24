# BPFtime Uprobe Benchmark Results

*Generated on 2026-09-09 17:03:18*

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
| __bench_uprobe | 1545.80 | 470.30 | 3.29x |
| __bench_uretprobe | 2081.20 | 467.41 | 4.45x |
| __bench_uprobe_uretprobe | 2230.11 | 540.94 | 4.12x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 4492.56 | 4518.59 | 4503.29 | 8.11 |
| __bench_array_map_delete | Userspace | 22675.73 | 22770.01 | 22734.28 | 29.88 |
| __bench_array_map_lookup | Kernel | 4781.06 | 4800.58 | 4790.07 | 7.21 |
| __bench_array_map_lookup | Userspace | 21639.27 | 21954.46 | 21829.84 | 107.14 |
| __bench_array_map_update | Kernel | 16700.21 | 16753.99 | 16718.54 | 16.41 |
| __bench_array_map_update | Userspace | 27922.07 | 28224.27 | 28057.42 | 94.63 |
| __bench_hash_map_delete | Kernel | 138090.51 | 138752.38 | 138253.90 | 189.62 |
| __bench_hash_map_delete | Userspace | 58132.46 | 59770.04 | 58855.44 | 685.00 |
| __bench_hash_map_lookup | Kernel | 36673.33 | 36923.10 | 36814.39 | 86.14 |
| __bench_hash_map_lookup | Userspace | 82385.27 | 83803.90 | 83194.90 | 563.31 |
| __bench_hash_map_update | Kernel | 120935.30 | 121138.16 | 121015.51 | 61.59 |
| __bench_hash_map_update | Userspace | 73111.67 | 73474.63 | 73305.77 | 116.25 |
| __bench_per_cpu_array_map_delete | Kernel | 4663.06 | 4713.92 | 4702.48 | 13.76 |
| __bench_per_cpu_array_map_delete | Userspace | 27077.29 | 27410.00 | 27212.91 | 131.99 |
| __bench_per_cpu_array_map_lookup | Kernel | 5622.72 | 5640.73 | 5631.05 | 5.59 |
| __bench_per_cpu_array_map_lookup | Userspace | 28737.60 | 29049.40 | 28892.80 | 113.12 |
| __bench_per_cpu_array_map_update | Kernel | 21245.86 | 21309.45 | 21282.03 | 23.65 |
| __bench_per_cpu_array_map_update | Userspace | 34137.42 | 34371.37 | 34277.08 | 73.40 |
| __bench_per_cpu_hash_map_delete | Kernel | 192614.53 | 193003.40 | 192752.58 | 128.87 |
| __bench_per_cpu_hash_map_delete | Userspace | 1121773.03 | 1136303.95 | 1124603.99 | 4005.33 |
| __bench_per_cpu_hash_map_lookup | Kernel | 36977.16 | 37214.59 | 37042.37 | 65.60 |
| __bench_per_cpu_hash_map_lookup | Userspace | 184367.66 | 185146.94 | 184645.99 | 249.63 |
| __bench_per_cpu_hash_map_update | Kernel | 86573.52 | 87022.27 | 86804.38 | 118.39 |
| __bench_per_cpu_hash_map_update | Userspace | 273035.37 | 274620.92 | 274036.70 | 521.35 |
| __bench_read | Kernel | 23026.11 | 23384.20 | 23255.80 | 96.61 |
| __bench_read | Userspace | 13205.15 | 13269.63 | 13236.59 | 19.32 |
| __bench_uprobe | Kernel | 1540.20 | 1551.49 | 1545.80 | 3.72 |
| __bench_uprobe | Userspace | 465.16 | 479.78 | 470.30 | 4.43 |
| __bench_uprobe_uretprobe | Kernel | 2210.81 | 2285.17 | 2230.11 | 27.45 |
| __bench_uprobe_uretprobe | Userspace | 536.25 | 546.48 | 540.94 | 3.72 |
| __bench_uretprobe | Kernel | 2069.88 | 2104.36 | 2081.20 | 11.40 |
| __bench_uretprobe | Userspace | 463.72 | 472.02 | 467.41 | 3.06 |
| __bench_write | Kernel | 22506.08 | 22793.66 | 22701.08 | 71.12 |
| __bench_write | Userspace | 13240.89 | 13560.69 | 13425.47 | 107.72 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 239.07 | 261.72 | 242.37 | 6.50 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-09-09 17:03:18
- **Total duration:** 5654.29 seconds

# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-14 10:30:56*

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
| __bench_uprobe | 1231.01 | 318.28 | 3.87x |
| __bench_uretprobe | 1655.34 | 317.25 | 5.22x |
| __bench_uprobe_uretprobe | 1760.83 | 334.39 | 5.27x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3683.54 | 3692.21 | 3687.08 | 3.48 |
| __bench_array_map_delete | Userspace | 16301.77 | 16413.75 | 16361.48 | 44.31 |
| __bench_array_map_lookup | Kernel | 3780.73 | 3790.98 | 3783.14 | 2.84 |
| __bench_array_map_lookup | Userspace | 16034.29 | 16364.18 | 16155.72 | 93.84 |
| __bench_array_map_update | Kernel | 13062.92 | 13103.92 | 13088.55 | 13.24 |
| __bench_array_map_update | Userspace | 23592.35 | 23794.07 | 23660.76 | 56.40 |
| __bench_hash_map_delete | Kernel | 107174.22 | 107263.98 | 107227.09 | 29.55 |
| __bench_hash_map_delete | Userspace | 41857.57 | 43728.38 | 42650.60 | 611.65 |
| __bench_hash_map_lookup | Kernel | 27994.59 | 28179.87 | 28064.92 | 68.10 |
| __bench_hash_map_lookup | Userspace | 57142.06 | 59252.35 | 58012.43 | 943.08 |
| __bench_hash_map_update | Kernel | 93252.72 | 93513.83 | 93370.94 | 77.61 |
| __bench_hash_map_update | Userspace | 61809.60 | 62193.24 | 61957.70 | 117.55 |
| __bench_per_cpu_array_map_delete | Kernel | 3684.94 | 3695.29 | 3689.66 | 3.92 |
| __bench_per_cpu_array_map_delete | Userspace | 16293.30 | 16438.64 | 16391.74 | 49.92 |
| __bench_per_cpu_array_map_lookup | Kernel | 4399.17 | 4408.31 | 4402.76 | 2.79 |
| __bench_per_cpu_array_map_lookup | Userspace | 21601.96 | 21744.83 | 21694.29 | 38.80 |
| __bench_per_cpu_array_map_update | Kernel | 16556.62 | 16621.40 | 16581.75 | 19.85 |
| __bench_per_cpu_array_map_update | Userspace | 27581.48 | 27706.76 | 27650.75 | 44.66 |
| __bench_per_cpu_hash_map_delete | Kernel | 150612.15 | 150722.92 | 150663.12 | 39.03 |
| __bench_per_cpu_hash_map_delete | Userspace | 755486.75 | 758245.61 | 756633.29 | 790.69 |
| __bench_per_cpu_hash_map_lookup | Kernel | 28884.14 | 29052.10 | 28978.95 | 61.35 |
| __bench_per_cpu_hash_map_lookup | Userspace | 62610.94 | 62994.51 | 62870.94 | 114.72 |
| __bench_per_cpu_hash_map_update | Kernel | 67586.98 | 67838.97 | 67716.81 | 67.16 |
| __bench_per_cpu_hash_map_update | Userspace | 71328.82 | 71789.93 | 71505.56 | 133.31 |
| __bench_read | Kernel | 17621.10 | 17726.44 | 17661.02 | 30.71 |
| __bench_read | Userspace | 4680.77 | 4716.51 | 4690.05 | 12.93 |
| __bench_uprobe | Kernel | 1224.64 | 1242.09 | 1231.01 | 5.32 |
| __bench_uprobe | Userspace | 315.76 | 320.66 | 318.28 | 1.54 |
| __bench_uprobe_uretprobe | Kernel | 1749.26 | 1784.40 | 1760.83 | 10.20 |
| __bench_uprobe_uretprobe | Userspace | 332.68 | 337.64 | 334.39 | 1.95 |
| __bench_uretprobe | Kernel | 1636.57 | 1706.24 | 1655.34 | 18.14 |
| __bench_uretprobe | Userspace | 314.83 | 320.19 | 317.25 | 1.97 |
| __bench_write | Kernel | 17783.45 | 19661.95 | 17988.79 | 557.86 |
| __bench_write | Userspace | 4395.32 | 4967.02 | 4577.99 | 234.36 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 158.75 | 185.53 | 164.73 | 7.58 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-14 10:30:56
- **Total duration:** 3289.59 seconds

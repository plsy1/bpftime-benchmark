# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-18 01:50:26*

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
| __bench_uprobe | 1226.25 | 193.56 | 6.34x |
| __bench_uretprobe | 1640.61 | 266.64 | 6.15x |
| __bench_uprobe_uretprobe | 1760.15 | 319.47 | 5.51x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3512.72 | 3528.83 | 3519.34 | 5.52 |
| __bench_array_map_delete | Userspace | 20041.99 | 20146.49 | 20106.88 | 33.05 |
| __bench_array_map_lookup | Kernel | 3782.16 | 3793.59 | 3786.55 | 4.08 |
| __bench_array_map_lookup | Userspace | 7895.16 | 7968.44 | 7952.50 | 21.66 |
| __bench_array_map_update | Kernel | 13003.80 | 13046.29 | 13020.40 | 11.08 |
| __bench_array_map_update | Userspace | 11529.39 | 11632.13 | 11571.25 | 31.98 |
| __bench_hash_map_delete | Kernel | 108133.66 | 108230.44 | 108164.06 | 25.88 |
| __bench_hash_map_delete | Userspace | 36236.44 | 36679.61 | 36443.21 | 128.03 |
| __bench_hash_map_lookup | Kernel | 28114.52 | 28314.27 | 28208.39 | 69.64 |
| __bench_hash_map_lookup | Userspace | 27923.28 | 28192.08 | 28116.34 | 72.06 |
| __bench_hash_map_update | Kernel | 94459.11 | 94669.51 | 94559.63 | 59.66 |
| __bench_hash_map_update | Userspace | 33266.90 | 36280.89 | 35453.61 | 1069.46 |
| __bench_per_cpu_array_map_delete | Kernel | 3682.27 | 3692.86 | 3685.16 | 3.37 |
| __bench_per_cpu_array_map_delete | Userspace | 20084.42 | 20153.52 | 20130.62 | 21.15 |
| __bench_per_cpu_array_map_lookup | Kernel | 4396.14 | 4403.43 | 4399.33 | 2.61 |
| __bench_per_cpu_array_map_lookup | Userspace | 11588.89 | 11701.84 | 11652.92 | 43.30 |
| __bench_per_cpu_array_map_update | Kernel | 16573.95 | 16667.08 | 16610.63 | 29.21 |
| __bench_per_cpu_array_map_update | Userspace | 14277.64 | 14411.42 | 14319.48 | 40.66 |
| __bench_per_cpu_hash_map_delete | Kernel | 152233.92 | 152434.95 | 152321.09 | 66.78 |
| __bench_per_cpu_hash_map_delete | Userspace | 98587.70 | 99315.56 | 98953.69 | 282.30 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29111.27 | 29293.10 | 29191.62 | 56.79 |
| __bench_per_cpu_hash_map_lookup | Userspace | 34797.68 | 35012.85 | 34924.05 | 67.46 |
| __bench_per_cpu_hash_map_update | Kernel | 68014.63 | 68109.72 | 68065.48 | 29.59 |
| __bench_per_cpu_hash_map_update | Userspace | 44403.92 | 44588.18 | 44460.43 | 50.60 |
| __bench_read | Kernel | 17615.85 | 17668.98 | 17640.30 | 16.65 |
| __bench_read | Userspace | 4582.23 | 4589.42 | 4585.85 | 2.43 |
| __bench_uprobe | Kernel | 1224.08 | 1231.38 | 1226.25 | 2.62 |
| __bench_uprobe | Userspace | 192.29 | 195.16 | 193.56 | 1.01 |
| __bench_uprobe_uretprobe | Kernel | 1754.83 | 1767.90 | 1760.15 | 4.71 |
| __bench_uprobe_uretprobe | Userspace | 318.38 | 320.44 | 319.47 | 0.55 |
| __bench_uretprobe | Kernel | 1637.13 | 1644.41 | 1640.61 | 2.40 |
| __bench_uretprobe | Userspace | 265.21 | 267.22 | 266.64 | 0.61 |
| __bench_write | Kernel | 17649.20 | 18245.73 | 17758.13 | 167.12 |
| __bench_write | Userspace | 4269.86 | 4271.10 | 4270.61 | 0.36 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 156.73 | 162.96 | 158.86 | 1.97 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-18 01:50:26
- **Total duration:** 1507.54 seconds

# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-19 21:46:00*

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
| __bench_uprobe | 1227.02 | 196.85 | 6.23x |
| __bench_uretprobe | 1641.96 | 267.98 | 6.13x |
| __bench_uprobe_uretprobe | 1754.58 | 318.82 | 5.50x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3682.81 | 3691.87 | 3686.71 | 3.37 |
| __bench_array_map_delete | Userspace | 20033.41 | 20280.78 | 20131.01 | 85.39 |
| __bench_array_map_lookup | Kernel | 3735.55 | 3746.19 | 3739.01 | 2.81 |
| __bench_array_map_lookup | Userspace | 6764.59 | 6776.51 | 6767.88 | 4.26 |
| __bench_array_map_update | Kernel | 13066.87 | 13114.22 | 13092.93 | 14.52 |
| __bench_array_map_update | Userspace | 11511.40 | 11566.88 | 11543.21 | 22.03 |
| __bench_hash_map_delete | Kernel | 107606.99 | 107722.93 | 107679.07 | 40.03 |
| __bench_hash_map_delete | Userspace | 36320.37 | 37990.30 | 36670.46 | 452.30 |
| __bench_hash_map_lookup | Kernel | 28175.95 | 28460.89 | 28277.51 | 80.61 |
| __bench_hash_map_lookup | Userspace | 26887.22 | 27379.94 | 27107.68 | 152.80 |
| __bench_hash_map_update | Kernel | 93240.89 | 93374.23 | 93286.91 | 38.88 |
| __bench_hash_map_update | Userspace | 33066.05 | 36141.88 | 33855.39 | 1102.90 |
| __bench_per_cpu_array_map_delete | Kernel | 3515.68 | 3534.52 | 3521.55 | 6.12 |
| __bench_per_cpu_array_map_delete | Userspace | 20048.58 | 21436.33 | 20293.67 | 391.19 |
| __bench_per_cpu_array_map_lookup | Kernel | 4216.45 | 4239.77 | 4221.07 | 6.52 |
| __bench_per_cpu_array_map_lookup | Userspace | 10715.81 | 10800.91 | 10759.06 | 23.64 |
| __bench_per_cpu_array_map_update | Kernel | 16629.66 | 16663.30 | 16645.24 | 11.11 |
| __bench_per_cpu_array_map_update | Userspace | 14382.58 | 15041.49 | 14527.82 | 199.11 |
| __bench_per_cpu_hash_map_delete | Kernel | 151479.20 | 151576.85 | 151521.03 | 35.06 |
| __bench_per_cpu_hash_map_delete | Userspace | 98522.49 | 99263.03 | 98696.74 | 271.31 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29416.15 | 29534.25 | 29471.78 | 46.90 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27514.94 | 27797.62 | 27708.70 | 78.75 |
| __bench_per_cpu_hash_map_update | Kernel | 67802.36 | 67894.41 | 67843.97 | 27.83 |
| __bench_per_cpu_hash_map_update | Userspace | 44339.91 | 44423.46 | 44378.82 | 24.54 |
| __bench_read | Kernel | 17998.39 | 18122.70 | 18088.51 | 34.58 |
| __bench_read | Userspace | 4584.59 | 4589.48 | 4586.76 | 1.76 |
| __bench_uprobe | Kernel | 1224.62 | 1234.14 | 1227.02 | 2.97 |
| __bench_uprobe | Userspace | 195.01 | 198.95 | 196.85 | 1.09 |
| __bench_uprobe_uretprobe | Kernel | 1749.21 | 1762.46 | 1754.58 | 3.26 |
| __bench_uprobe_uretprobe | Userspace | 317.80 | 319.50 | 318.82 | 0.49 |
| __bench_uretprobe | Kernel | 1636.71 | 1653.01 | 1641.96 | 4.87 |
| __bench_uretprobe | Userspace | 266.56 | 269.39 | 267.98 | 0.75 |
| __bench_write | Kernel | 17585.06 | 17774.94 | 17680.89 | 53.34 |
| __bench_write | Userspace | 4270.73 | 4274.22 | 4271.89 | 1.00 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 155.27 | 182.43 | 161.83 | 7.42 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-19 21:46:00
- **Total duration:** 1490.34 seconds

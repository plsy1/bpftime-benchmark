# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-15 00:07:41*

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
| __bench_uprobe | 1227.89 | 195.79 | 6.27x |
| __bench_uretprobe | 1643.62 | 267.86 | 6.14x |
| __bench_uprobe_uretprobe | 1754.92 | 318.85 | 5.50x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3691.34 | 3702.14 | 3697.91 | 4.18 |
| __bench_array_map_delete | Userspace | 20253.00 | 20606.62 | 20430.66 | 137.05 |
| __bench_array_map_lookup | Kernel | 3772.54 | 3790.45 | 3780.07 | 5.96 |
| __bench_array_map_lookup | Userspace | 19945.11 | 20095.03 | 20028.90 | 47.33 |
| __bench_array_map_update | Kernel | 13084.88 | 13137.16 | 13103.32 | 16.11 |
| __bench_array_map_update | Userspace | 25297.84 | 26563.84 | 25458.97 | 368.65 |
| __bench_hash_map_delete | Kernel | 52434.50 | 52513.99 | 52475.10 | 19.39 |
| __bench_hash_map_delete | Userspace | 150481.15 | 152741.96 | 150948.74 | 609.49 |
| __bench_hash_map_lookup | Kernel | 28541.22 | 28668.63 | 28598.33 | 46.44 |
| __bench_hash_map_lookup | Userspace | 51844.26 | 53689.28 | 52398.49 | 672.60 |
| __bench_hash_map_update | Kernel | 93659.22 | 93751.02 | 93698.82 | 33.97 |
| __bench_hash_map_update | Userspace | 61597.95 | 63092.09 | 61997.31 | 410.32 |
| __bench_per_cpu_array_map_delete | Kernel | 3682.98 | 3691.43 | 3685.73 | 2.08 |
| __bench_per_cpu_array_map_delete | Userspace | 20147.01 | 20549.99 | 20384.60 | 144.52 |
| __bench_per_cpu_array_map_lookup | Kernel | 4396.93 | 4410.77 | 4401.37 | 4.54 |
| __bench_per_cpu_array_map_lookup | Userspace | 28761.06 | 29104.19 | 28869.98 | 120.56 |
| __bench_per_cpu_array_map_update | Kernel | 16546.19 | 16591.17 | 16568.13 | 16.88 |
| __bench_per_cpu_array_map_update | Userspace | 74028.59 | 74202.91 | 74087.65 | 46.98 |
| __bench_per_cpu_hash_map_delete | Kernel | 52234.13 | 52310.64 | 52257.95 | 22.17 |
| __bench_per_cpu_hash_map_delete | Userspace | 254339.49 | 256942.01 | 256233.48 | 683.48 |
| __bench_per_cpu_hash_map_lookup | Kernel | 28977.07 | 29339.47 | 29153.43 | 132.01 |
| __bench_per_cpu_hash_map_lookup | Userspace | 124816.75 | 125259.99 | 124994.78 | 140.17 |
| __bench_per_cpu_hash_map_update | Kernel | 68035.91 | 68232.48 | 68141.19 | 76.73 |
| __bench_per_cpu_hash_map_update | Userspace | 208336.60 | 212220.78 | 211484.53 | 1106.39 |
| __bench_read | Kernel | 17997.36 | 18202.20 | 18107.63 | 57.23 |
| __bench_read | Userspace | 4568.72 | 4595.18 | 4578.86 | 8.51 |
| __bench_uprobe | Kernel | 1223.72 | 1248.00 | 1227.89 | 6.84 |
| __bench_uprobe | Userspace | 194.62 | 197.56 | 195.79 | 0.93 |
| __bench_uprobe_uretprobe | Kernel | 1748.52 | 1763.49 | 1754.92 | 5.11 |
| __bench_uprobe_uretprobe | Userspace | 317.71 | 323.87 | 318.85 | 1.73 |
| __bench_uretprobe | Kernel | 1635.25 | 1677.24 | 1643.62 | 11.90 |
| __bench_uretprobe | Userspace | 266.54 | 271.21 | 267.86 | 1.35 |
| __bench_write | Kernel | 17755.75 | 17844.69 | 17789.58 | 25.87 |
| __bench_write | Userspace | 4270.41 | 4274.02 | 4271.56 | 1.10 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 153.06 | 180.28 | 158.61 | 7.84 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-15 00:07:41
- **Total duration:** 1482.43 seconds

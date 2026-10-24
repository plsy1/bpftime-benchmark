# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-15 17:04:35*

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
| __bench_uprobe | 1229.67 | 196.38 | 6.26x |
| __bench_uretprobe | 1643.09 | 267.16 | 6.15x |
| __bench_uprobe_uretprobe | 1757.43 | 318.39 | 5.52x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3675.51 | 3686.21 | 3681.16 | 4.28 |
| __bench_array_map_delete | Userspace | 24585.61 | 24914.08 | 24702.67 | 91.03 |
| __bench_array_map_lookup | Kernel | 3806.78 | 3824.43 | 3814.57 | 5.44 |
| __bench_array_map_lookup | Userspace | 24271.63 | 24459.57 | 24386.66 | 64.69 |
| __bench_array_map_update | Kernel | 13193.61 | 13225.90 | 13213.48 | 9.45 |
| __bench_array_map_update | Userspace | 32176.94 | 32360.52 | 32272.15 | 56.62 |
| __bench_hash_map_delete | Kernel | 107022.90 | 107173.45 | 107126.52 | 41.74 |
| __bench_hash_map_delete | Userspace | 59813.70 | 60889.18 | 60449.92 | 439.80 |
| __bench_hash_map_lookup | Kernel | 27957.35 | 28400.58 | 28272.93 | 129.25 |
| __bench_hash_map_lookup | Userspace | 62649.97 | 64150.64 | 63766.48 | 553.42 |
| __bench_hash_map_update | Kernel | 92960.45 | 93034.31 | 92990.11 | 23.66 |
| __bench_hash_map_update | Userspace | 67717.21 | 68755.50 | 67979.34 | 271.72 |
| __bench_per_cpu_array_map_delete | Kernel | 3683.91 | 3709.55 | 3689.81 | 7.22 |
| __bench_per_cpu_array_map_delete | Userspace | 24606.38 | 24763.46 | 24664.74 | 54.85 |
| __bench_per_cpu_array_map_lookup | Kernel | 4397.97 | 4421.50 | 4407.19 | 6.23 |
| __bench_per_cpu_array_map_lookup | Userspace | 14326.03 | 14488.91 | 14410.72 | 48.28 |
| __bench_per_cpu_array_map_update | Kernel | 16582.28 | 16619.07 | 16599.75 | 11.11 |
| __bench_per_cpu_array_map_update | Userspace | 35661.61 | 36035.63 | 35813.47 | 121.41 |
| __bench_per_cpu_hash_map_delete | Kernel | 149810.60 | 149942.63 | 149864.07 | 41.79 |
| __bench_per_cpu_hash_map_delete | Userspace | 117926.62 | 118923.24 | 118334.73 | 257.72 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29054.36 | 29429.80 | 29231.08 | 94.98 |
| __bench_per_cpu_hash_map_lookup | Userspace | 72199.87 | 72515.79 | 72355.00 | 85.43 |
| __bench_per_cpu_hash_map_update | Kernel | 67797.93 | 67912.32 | 67855.77 | 38.45 |
| __bench_per_cpu_hash_map_update | Userspace | 79974.64 | 80371.93 | 80160.36 | 110.83 |
| __bench_read | Kernel | 18044.77 | 18184.67 | 18117.22 | 33.39 |
| __bench_read | Userspace | 4571.21 | 4587.11 | 4577.18 | 5.44 |
| __bench_uprobe | Kernel | 1225.38 | 1234.13 | 1229.67 | 3.26 |
| __bench_uprobe | Userspace | 194.58 | 198.94 | 196.38 | 1.46 |
| __bench_uprobe_uretprobe | Kernel | 1750.03 | 1771.62 | 1757.43 | 7.92 |
| __bench_uprobe_uretprobe | Userspace | 316.32 | 326.54 | 318.39 | 2.90 |
| __bench_uretprobe | Kernel | 1636.16 | 1660.75 | 1643.09 | 8.58 |
| __bench_uretprobe | Userspace | 265.54 | 271.43 | 267.16 | 1.71 |
| __bench_write | Kernel | 17583.34 | 18553.43 | 17780.50 | 260.56 |
| __bench_write | Userspace | 4271.48 | 4737.70 | 4399.49 | 171.50 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 154.65 | 175.27 | 157.46 | 5.98 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-15 17:04:35
- **Total duration:** 1924.09 seconds

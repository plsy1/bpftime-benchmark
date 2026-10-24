# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-15 23:14:42*

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
| __bench_uprobe | 1226.49 | 197.37 | 6.21x |
| __bench_uretprobe | 1638.24 | 269.08 | 6.09x |
| __bench_uprobe_uretprobe | 1755.87 | 322.49 | 5.44x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3682.78 | 3698.19 | 3688.84 | 5.67 |
| __bench_array_map_delete | Userspace | 20250.09 | 20509.69 | 20389.52 | 78.23 |
| __bench_array_map_lookup | Kernel | 3804.15 | 3813.57 | 3807.13 | 3.14 |
| __bench_array_map_lookup | Userspace | 10849.62 | 10917.07 | 10887.88 | 18.37 |
| __bench_array_map_update | Kernel | 13011.98 | 13055.04 | 13034.55 | 13.95 |
| __bench_array_map_update | Userspace | 13894.77 | 14050.59 | 13962.75 | 52.33 |
| __bench_hash_map_delete | Kernel | 107679.53 | 107794.48 | 107719.47 | 34.70 |
| __bench_hash_map_delete | Userspace | 47012.02 | 47340.27 | 47158.98 | 128.17 |
| __bench_hash_map_lookup | Kernel | 28236.56 | 28454.16 | 28338.89 | 71.53 |
| __bench_hash_map_lookup | Userspace | 60765.83 | 61880.81 | 61201.53 | 394.28 |
| __bench_hash_map_update | Kernel | 93965.54 | 94179.35 | 94087.55 | 70.59 |
| __bench_hash_map_update | Userspace | 64567.33 | 65335.38 | 64965.91 | 230.71 |
| __bench_per_cpu_array_map_delete | Kernel | 3517.57 | 3526.45 | 3521.70 | 2.96 |
| __bench_per_cpu_array_map_delete | Userspace | 20239.36 | 20425.27 | 20354.54 | 58.16 |
| __bench_per_cpu_array_map_lookup | Kernel | 4219.31 | 4226.42 | 4222.65 | 2.31 |
| __bench_per_cpu_array_map_lookup | Userspace | 14650.13 | 14795.22 | 14731.30 | 38.92 |
| __bench_per_cpu_array_map_update | Kernel | 16601.49 | 16653.17 | 16629.27 | 15.10 |
| __bench_per_cpu_array_map_update | Userspace | 28831.48 | 29009.74 | 28923.37 | 60.48 |
| __bench_per_cpu_hash_map_delete | Kernel | 149353.38 | 149484.01 | 149408.41 | 39.91 |
| __bench_per_cpu_hash_map_delete | Userspace | 98733.84 | 99483.77 | 99035.48 | 266.33 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29093.27 | 29254.02 | 29188.54 | 47.24 |
| __bench_per_cpu_hash_map_lookup | Userspace | 50061.14 | 50434.36 | 50270.10 | 129.11 |
| __bench_per_cpu_hash_map_update | Kernel | 67389.17 | 67532.31 | 67461.47 | 41.27 |
| __bench_per_cpu_hash_map_update | Userspace | 65162.88 | 65694.55 | 65484.00 | 179.22 |
| __bench_read | Kernel | 18033.50 | 18204.24 | 18114.97 | 51.33 |
| __bench_read | Userspace | 4569.07 | 4582.12 | 4575.25 | 4.07 |
| __bench_uprobe | Kernel | 1225.05 | 1228.98 | 1226.49 | 1.11 |
| __bench_uprobe | Userspace | 194.66 | 200.55 | 197.37 | 2.04 |
| __bench_uprobe_uretprobe | Kernel | 1749.92 | 1761.22 | 1755.87 | 3.53 |
| __bench_uprobe_uretprobe | Userspace | 318.58 | 344.46 | 322.49 | 7.46 |
| __bench_uretprobe | Kernel | 1630.92 | 1650.11 | 1638.24 | 5.26 |
| __bench_uretprobe | Userspace | 267.02 | 271.01 | 269.08 | 1.43 |
| __bench_write | Kernel | 17663.26 | 17858.02 | 17720.87 | 57.79 |
| __bench_write | Userspace | 4271.20 | 4274.43 | 4272.89 | 1.05 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 151.34 | 171.93 | 155.80 | 5.92 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-15 23:14:42
- **Total duration:** 1764.31 seconds

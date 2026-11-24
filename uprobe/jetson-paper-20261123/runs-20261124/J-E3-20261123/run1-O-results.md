# BPFtime Uprobe Benchmark Results

*Generated on 2026-11-23 19:54:05*

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
| __bench_uprobe | 1243.11 | 197.13 | 6.31x |
| __bench_uretprobe | 1677.90 | 267.97 | 6.26x |
| __bench_uprobe_uretprobe | 1793.01 | 325.57 | 5.51x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3743.75 | 3758.58 | 3748.45 | 4.53 |
| __bench_array_map_delete | Userspace | 20222.25 | 20368.30 | 20283.21 | 56.92 |
| __bench_array_map_lookup | Kernel | 3817.93 | 3833.73 | 3823.01 | 4.88 |
| __bench_array_map_lookup | Userspace | 19944.54 | 20113.05 | 20020.08 | 57.25 |
| __bench_array_map_update | Kernel | 13121.09 | 13157.15 | 13142.77 | 10.36 |
| __bench_array_map_update | Userspace | 25407.17 | 25903.57 | 25551.43 | 175.07 |
| __bench_hash_map_delete | Kernel | 107096.88 | 107213.52 | 107163.81 | 38.14 |
| __bench_hash_map_delete | Userspace | 47243.04 | 48396.42 | 47628.43 | 328.72 |
| __bench_hash_map_lookup | Kernel | 28922.80 | 29287.32 | 29043.76 | 99.74 |
| __bench_hash_map_lookup | Userspace | 51654.89 | 57948.61 | 52688.62 | 1792.26 |
| __bench_hash_map_update | Kernel | 96023.85 | 96240.40 | 96090.38 | 67.30 |
| __bench_hash_map_update | Userspace | 61365.04 | 62869.42 | 61938.68 | 474.44 |
| __bench_per_cpu_array_map_delete | Kernel | 3744.47 | 3757.66 | 3749.48 | 5.19 |
| __bench_per_cpu_array_map_delete | Userspace | 20240.14 | 20447.01 | 20328.26 | 62.41 |
| __bench_per_cpu_array_map_lookup | Kernel | 4522.54 | 4529.48 | 4524.84 | 2.44 |
| __bench_per_cpu_array_map_lookup | Userspace | 28474.04 | 28898.89 | 28683.45 | 133.26 |
| __bench_per_cpu_array_map_update | Kernel | 16639.11 | 16680.02 | 16657.20 | 12.26 |
| __bench_per_cpu_array_map_update | Userspace | 71933.13 | 72484.49 | 72223.14 | 157.48 |
| __bench_per_cpu_hash_map_delete | Kernel | 149012.75 | 149200.81 | 149085.62 | 56.81 |
| __bench_per_cpu_hash_map_delete | Userspace | 256384.22 | 257087.71 | 256758.88 | 200.52 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29154.71 | 29377.47 | 29271.44 | 60.19 |
| __bench_per_cpu_hash_map_lookup | Userspace | 127857.59 | 128574.98 | 128188.74 | 223.03 |
| __bench_per_cpu_hash_map_update | Kernel | 67986.04 | 68121.31 | 68039.51 | 37.09 |
| __bench_per_cpu_hash_map_update | Userspace | 206981.45 | 210471.52 | 209423.90 | 1160.42 |
| __bench_read | Kernel | 17534.00 | 17642.99 | 17608.38 | 30.58 |
| __bench_read | Userspace | 4578.73 | 4586.67 | 4580.37 | 2.17 |
| __bench_uprobe | Kernel | 1239.60 | 1254.34 | 1243.11 | 4.89 |
| __bench_uprobe | Userspace | 195.40 | 198.65 | 197.13 | 0.90 |
| __bench_uprobe_uretprobe | Kernel | 1773.41 | 1809.63 | 1793.01 | 9.83 |
| __bench_uprobe_uretprobe | Userspace | 318.47 | 338.20 | 325.57 | 6.91 |
| __bench_uretprobe | Kernel | 1658.25 | 1696.66 | 1677.90 | 9.99 |
| __bench_uretprobe | Userspace | 265.93 | 272.56 | 267.97 | 1.75 |
| __bench_write | Kernel | 17824.14 | 18423.49 | 17903.72 | 173.95 |
| __bench_write | Userspace | 4270.37 | 4274.81 | 4271.77 | 1.17 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.71 | 175.99 | 157.18 | 6.44 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-11-23 19:54:05
- **Total duration:** 2250.80 seconds

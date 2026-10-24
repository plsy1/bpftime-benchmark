# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-15 21:57:55*

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
| __bench_uprobe | 1228.31 | 196.59 | 6.25x |
| __bench_uretprobe | 1642.05 | 265.56 | 6.18x |
| __bench_uprobe_uretprobe | 1753.30 | 318.78 | 5.50x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3681.37 | 3701.53 | 3688.87 | 6.28 |
| __bench_array_map_delete | Userspace | 24652.55 | 25086.73 | 24827.44 | 119.09 |
| __bench_array_map_lookup | Kernel | 3805.76 | 3816.70 | 3810.78 | 4.11 |
| __bench_array_map_lookup | Userspace | 10672.06 | 10795.94 | 10715.27 | 30.75 |
| __bench_array_map_update | Kernel | 13021.38 | 13053.64 | 13039.81 | 11.22 |
| __bench_array_map_update | Userspace | 15955.88 | 16092.71 | 16046.59 | 44.66 |
| __bench_hash_map_delete | Kernel | 107922.93 | 108145.39 | 108025.57 | 71.63 |
| __bench_hash_map_delete | Userspace | 60031.22 | 60949.67 | 60462.50 | 351.04 |
| __bench_hash_map_lookup | Kernel | 28153.65 | 28307.63 | 28223.93 | 50.55 |
| __bench_hash_map_lookup | Userspace | 62778.98 | 64356.46 | 63371.69 | 642.52 |
| __bench_hash_map_update | Kernel | 93070.09 | 93238.74 | 93149.28 | 50.53 |
| __bench_hash_map_update | Userspace | 70240.18 | 71384.56 | 70790.70 | 390.65 |
| __bench_per_cpu_array_map_delete | Kernel | 3508.56 | 3533.24 | 3522.78 | 6.61 |
| __bench_per_cpu_array_map_delete | Userspace | 24708.48 | 25101.79 | 24830.89 | 145.68 |
| __bench_per_cpu_array_map_lookup | Kernel | 4208.94 | 4223.88 | 4218.37 | 3.97 |
| __bench_per_cpu_array_map_lookup | Userspace | 14308.21 | 14566.08 | 14381.01 | 71.78 |
| __bench_per_cpu_array_map_update | Kernel | 16616.50 | 16663.13 | 16634.44 | 14.59 |
| __bench_per_cpu_array_map_update | Userspace | 35531.54 | 35868.22 | 35729.50 | 116.55 |
| __bench_per_cpu_hash_map_delete | Kernel | 149028.79 | 149269.82 | 149128.11 | 72.92 |
| __bench_per_cpu_hash_map_delete | Userspace | 117933.15 | 119869.13 | 118324.80 | 530.62 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29128.18 | 29320.08 | 29251.81 | 53.66 |
| __bench_per_cpu_hash_map_lookup | Userspace | 72166.15 | 72531.34 | 72293.83 | 109.83 |
| __bench_per_cpu_hash_map_update | Kernel | 67323.67 | 67489.02 | 67386.50 | 49.64 |
| __bench_per_cpu_hash_map_update | Userspace | 79563.21 | 80372.21 | 79978.51 | 239.02 |
| __bench_read | Kernel | 18008.03 | 18112.91 | 18076.74 | 37.11 |
| __bench_read | Userspace | 4566.02 | 4573.31 | 4570.48 | 2.51 |
| __bench_uprobe | Kernel | 1224.82 | 1236.35 | 1228.31 | 3.63 |
| __bench_uprobe | Userspace | 194.55 | 198.39 | 196.59 | 1.21 |
| __bench_uprobe_uretprobe | Kernel | 1747.77 | 1758.66 | 1753.30 | 4.07 |
| __bench_uprobe_uretprobe | Userspace | 317.63 | 319.78 | 318.78 | 0.72 |
| __bench_uretprobe | Kernel | 1637.25 | 1651.18 | 1642.05 | 3.71 |
| __bench_uretprobe | Userspace | 264.00 | 266.84 | 265.56 | 0.95 |
| __bench_write | Kernel | 17681.79 | 17751.75 | 17717.77 | 20.80 |
| __bench_write | Userspace | 4268.79 | 4276.58 | 4271.42 | 1.96 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.77 | 177.86 | 157.58 | 7.23 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-15 21:57:55
- **Total duration:** 1896.58 seconds

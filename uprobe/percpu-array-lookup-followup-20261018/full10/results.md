# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-18 22:42:49*

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
| __bench_uprobe | 1233.46 | 195.55 | 6.31x |
| __bench_uretprobe | 1657.56 | 266.76 | 6.21x |
| __bench_uprobe_uretprobe | 1767.37 | 317.02 | 5.57x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3683.71 | 3696.50 | 3687.44 | 4.24 |
| __bench_array_map_delete | Userspace | 20050.87 | 20216.89 | 20130.08 | 52.62 |
| __bench_array_map_lookup | Kernel | 3791.78 | 3815.16 | 3799.47 | 6.42 |
| __bench_array_map_lookup | Userspace | 7918.59 | 7948.08 | 7930.49 | 11.11 |
| __bench_array_map_update | Kernel | 13062.00 | 13126.22 | 13084.18 | 17.79 |
| __bench_array_map_update | Userspace | 11515.96 | 11582.21 | 11535.05 | 20.24 |
| __bench_hash_map_delete | Kernel | 107898.72 | 107962.97 | 107936.52 | 19.18 |
| __bench_hash_map_delete | Userspace | 36320.97 | 36696.62 | 36512.50 | 135.35 |
| __bench_hash_map_lookup | Kernel | 28203.49 | 28534.43 | 28362.65 | 91.15 |
| __bench_hash_map_lookup | Userspace | 26797.38 | 27124.05 | 26987.66 | 121.99 |
| __bench_hash_map_update | Kernel | 93919.62 | 94079.49 | 94021.85 | 40.78 |
| __bench_hash_map_update | Userspace | 33063.46 | 36755.23 | 35015.18 | 1499.11 |
| __bench_per_cpu_array_map_delete | Kernel | 3683.40 | 3696.31 | 3687.74 | 4.08 |
| __bench_per_cpu_array_map_delete | Userspace | 20063.81 | 20269.19 | 20160.24 | 60.65 |
| __bench_per_cpu_array_map_lookup | Kernel | 4403.30 | 4417.60 | 4410.03 | 4.41 |
| __bench_per_cpu_array_map_lookup | Userspace | 10898.65 | 10971.47 | 10923.65 | 23.59 |
| __bench_per_cpu_array_map_update | Kernel | 16585.74 | 16638.00 | 16612.21 | 17.08 |
| __bench_per_cpu_array_map_update | Userspace | 14377.70 | 14497.08 | 14441.58 | 33.95 |
| __bench_per_cpu_hash_map_delete | Kernel | 150904.05 | 151071.73 | 150982.04 | 56.16 |
| __bench_per_cpu_hash_map_delete | Userspace | 98475.93 | 101088.84 | 99057.25 | 744.53 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29001.52 | 29323.41 | 29163.09 | 118.43 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27554.22 | 27801.76 | 27695.98 | 75.58 |
| __bench_per_cpu_hash_map_update | Kernel | 68216.53 | 68313.04 | 68260.13 | 36.56 |
| __bench_per_cpu_hash_map_update | Userspace | 44436.14 | 44846.40 | 44546.71 | 109.18 |
| __bench_read | Kernel | 18030.13 | 18132.49 | 18093.52 | 31.85 |
| __bench_read | Userspace | 4607.05 | 4613.09 | 4610.13 | 1.85 |
| __bench_uprobe | Kernel | 1224.83 | 1292.31 | 1233.46 | 19.77 |
| __bench_uprobe | Userspace | 194.01 | 197.22 | 195.55 | 1.15 |
| __bench_uprobe_uretprobe | Kernel | 1751.22 | 1775.45 | 1767.37 | 7.67 |
| __bench_uprobe_uretprobe | Userspace | 315.31 | 319.67 | 317.02 | 1.29 |
| __bench_uretprobe | Kernel | 1637.63 | 1672.10 | 1657.56 | 8.14 |
| __bench_uretprobe | Userspace | 264.69 | 269.08 | 266.76 | 1.46 |
| __bench_write | Kernel | 17610.13 | 17765.40 | 17718.93 | 41.56 |
| __bench_write | Userspace | 4267.87 | 4825.62 | 4377.42 | 191.05 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 154.57 | 178.50 | 159.77 | 6.90 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-18 22:42:49
- **Total duration:** 1497.11 seconds

# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-17 14:52:48*

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
| __bench_uprobe | 1227.22 | 196.99 | 6.23x |
| __bench_uretprobe | 1641.99 | 268.48 | 6.12x |
| __bench_uprobe_uretprobe | 1754.96 | 318.94 | 5.50x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3643.69 | 3696.99 | 3682.13 | 13.56 |
| __bench_array_map_delete | Userspace | 20028.00 | 20230.25 | 20158.95 | 71.15 |
| __bench_array_map_lookup | Kernel | 3804.80 | 3822.95 | 3810.43 | 5.98 |
| __bench_array_map_lookup | Userspace | 7900.53 | 7949.46 | 7925.81 | 13.19 |
| __bench_array_map_update | Kernel | 13087.12 | 13132.74 | 13104.49 | 14.70 |
| __bench_array_map_update | Userspace | 11466.17 | 11709.42 | 11527.32 | 65.01 |
| __bench_hash_map_delete | Kernel | 107936.33 | 108221.77 | 108096.17 | 74.79 |
| __bench_hash_map_delete | Userspace | 39634.28 | 40269.78 | 39803.58 | 189.09 |
| __bench_hash_map_lookup | Kernel | 27596.29 | 27895.27 | 27734.49 | 73.46 |
| __bench_hash_map_lookup | Userspace | 37742.76 | 38287.69 | 37893.65 | 197.74 |
| __bench_hash_map_update | Kernel | 94519.11 | 94633.83 | 94576.36 | 29.16 |
| __bench_hash_map_update | Userspace | 56513.80 | 57347.02 | 56923.51 | 273.46 |
| __bench_per_cpu_array_map_delete | Kernel | 3682.56 | 3694.10 | 3687.51 | 4.29 |
| __bench_per_cpu_array_map_delete | Userspace | 20121.93 | 20245.69 | 20170.96 | 35.71 |
| __bench_per_cpu_array_map_lookup | Kernel | 4400.43 | 4419.34 | 4405.83 | 5.29 |
| __bench_per_cpu_array_map_lookup | Userspace | 11573.71 | 11697.88 | 11621.63 | 47.52 |
| __bench_per_cpu_array_map_update | Kernel | 16585.70 | 16670.01 | 16625.67 | 22.98 |
| __bench_per_cpu_array_map_update | Userspace | 27654.84 | 27930.30 | 27770.83 | 91.92 |
| __bench_per_cpu_hash_map_delete | Kernel | 150275.35 | 150421.38 | 150341.92 | 47.09 |
| __bench_per_cpu_hash_map_delete | Userspace | 98632.60 | 99612.83 | 99050.44 | 355.18 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29228.12 | 29412.57 | 29321.62 | 56.83 |
| __bench_per_cpu_hash_map_lookup | Userspace | 41649.56 | 41825.61 | 41749.10 | 60.13 |
| __bench_per_cpu_hash_map_update | Kernel | 68153.03 | 68275.42 | 68185.83 | 38.47 |
| __bench_per_cpu_hash_map_update | Userspace | 61329.18 | 61956.98 | 61696.60 | 183.29 |
| __bench_read | Kernel | 17999.02 | 18200.13 | 18109.49 | 56.91 |
| __bench_read | Userspace | 4568.72 | 4585.24 | 4574.05 | 4.33 |
| __bench_uprobe | Kernel | 1223.78 | 1232.64 | 1227.22 | 2.91 |
| __bench_uprobe | Userspace | 195.70 | 198.50 | 196.99 | 0.92 |
| __bench_uprobe_uretprobe | Kernel | 1749.80 | 1761.79 | 1754.96 | 3.98 |
| __bench_uprobe_uretprobe | Userspace | 317.87 | 321.27 | 318.94 | 1.04 |
| __bench_uretprobe | Kernel | 1637.09 | 1651.84 | 1641.99 | 5.05 |
| __bench_uretprobe | Userspace | 267.22 | 269.29 | 268.48 | 0.77 |
| __bench_write | Kernel | 17570.28 | 17689.75 | 17642.34 | 39.75 |
| __bench_write | Userspace | 4274.25 | 4845.62 | 4538.71 | 228.34 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 153.90 | 176.31 | 160.56 | 6.03 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-17 14:52:48
- **Total duration:** 1638.21 seconds

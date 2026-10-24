# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-11 23:15:46*

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
| __bench_uprobe | 1228.53 | 317.78 | 3.87x |
| __bench_uretprobe | 1640.79 | 317.91 | 5.16x |
| __bench_uprobe_uretprobe | 1753.42 | 340.38 | 5.15x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3688.88 | 3699.41 | 3693.04 | 3.40 |
| __bench_array_map_delete | Userspace | 16296.75 | 16411.40 | 16353.08 | 37.98 |
| __bench_array_map_lookup | Kernel | 3728.99 | 3814.72 | 3801.02 | 24.24 |
| __bench_array_map_lookup | Userspace | 16059.28 | 16273.82 | 16128.78 | 70.36 |
| __bench_array_map_update | Kernel | 13054.01 | 13109.32 | 13085.39 | 15.38 |
| __bench_array_map_update | Userspace | 23417.70 | 23722.55 | 23517.46 | 107.76 |
| __bench_hash_map_delete | Kernel | 106789.66 | 106980.85 | 106889.88 | 65.30 |
| __bench_hash_map_delete | Userspace | 41773.38 | 43661.39 | 42519.14 | 686.06 |
| __bench_hash_map_lookup | Kernel | 27979.46 | 28370.62 | 28193.52 | 103.86 |
| __bench_hash_map_lookup | Userspace | 57268.64 | 59401.02 | 58095.38 | 871.47 |
| __bench_hash_map_update | Kernel | 93346.18 | 93510.39 | 93442.51 | 61.69 |
| __bench_hash_map_update | Userspace | 62262.12 | 64077.21 | 63268.16 | 691.56 |
| __bench_per_cpu_array_map_delete | Kernel | 3680.05 | 3688.24 | 3683.23 | 2.10 |
| __bench_per_cpu_array_map_delete | Userspace | 16316.83 | 16480.06 | 16397.48 | 59.57 |
| __bench_per_cpu_array_map_lookup | Kernel | 4398.00 | 4400.39 | 4398.93 | 0.85 |
| __bench_per_cpu_array_map_lookup | Userspace | 21566.39 | 21767.11 | 21683.41 | 64.94 |
| __bench_per_cpu_array_map_update | Kernel | 16604.37 | 16653.25 | 16627.33 | 16.58 |
| __bench_per_cpu_array_map_update | Userspace | 27111.72 | 27557.89 | 27351.71 | 126.97 |
| __bench_per_cpu_hash_map_delete | Kernel | 151101.69 | 151304.35 | 151204.43 | 55.54 |
| __bench_per_cpu_hash_map_delete | Userspace | 983471.27 | 995736.84 | 986411.11 | 3831.96 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29234.83 | 29404.06 | 29334.03 | 58.67 |
| __bench_per_cpu_hash_map_lookup | Userspace | 136880.56 | 137896.69 | 137364.24 | 285.48 |
| __bench_per_cpu_hash_map_update | Kernel | 67399.04 | 67519.56 | 67464.05 | 33.19 |
| __bench_per_cpu_hash_map_update | Userspace | 219278.69 | 221821.86 | 221234.15 | 677.74 |
| __bench_read | Kernel | 17981.28 | 18147.09 | 18076.47 | 43.67 |
| __bench_read | Userspace | 4680.56 | 4687.90 | 4682.97 | 2.06 |
| __bench_uprobe | Kernel | 1223.57 | 1233.66 | 1228.53 | 3.62 |
| __bench_uprobe | Userspace | 315.86 | 319.90 | 317.78 | 1.39 |
| __bench_uprobe_uretprobe | Kernel | 1745.10 | 1765.01 | 1753.42 | 5.31 |
| __bench_uprobe_uretprobe | Userspace | 334.50 | 350.41 | 340.38 | 5.67 |
| __bench_uretprobe | Kernel | 1635.21 | 1650.16 | 1640.79 | 4.36 |
| __bench_uretprobe | Userspace | 316.19 | 320.62 | 317.91 | 1.55 |
| __bench_write | Kernel | 17543.53 | 17805.13 | 17681.40 | 73.46 |
| __bench_write | Userspace | 4391.87 | 4396.59 | 4394.65 | 1.31 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 161.00 | 185.52 | 166.69 | 6.89 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-11 23:15:46
- **Total duration:** 4691.97 seconds

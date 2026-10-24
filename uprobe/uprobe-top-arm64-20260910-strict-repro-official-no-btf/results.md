# BPFtime Uprobe Benchmark Results

*Generated on 2026-09-10 13:34:37*

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
| __bench_uprobe | 1232.44 | 318.60 | 3.87x |
| __bench_uretprobe | 1647.03 | 325.38 | 5.06x |
| __bench_uprobe_uretprobe | 1756.97 | 334.77 | 5.25x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3515.24 | 3524.84 | 3518.96 | 3.66 |
| __bench_array_map_delete | Userspace | 16314.02 | 16495.51 | 16381.87 | 53.38 |
| __bench_array_map_lookup | Kernel | 3782.36 | 3791.88 | 3788.01 | 3.81 |
| __bench_array_map_lookup | Userspace | 15817.15 | 16100.56 | 15941.19 | 86.80 |
| __bench_array_map_update | Kernel | 13080.37 | 13133.59 | 13106.37 | 16.39 |
| __bench_array_map_update | Userspace | 23365.08 | 23545.94 | 23458.84 | 53.06 |
| __bench_hash_map_delete | Kernel | 109085.88 | 109284.21 | 109183.38 | 49.84 |
| __bench_hash_map_delete | Userspace | 41852.15 | 43709.52 | 42363.26 | 546.86 |
| __bench_hash_map_lookup | Kernel | 28435.91 | 28743.22 | 28557.44 | 84.79 |
| __bench_hash_map_lookup | Userspace | 57627.25 | 59665.57 | 58104.22 | 739.02 |
| __bench_hash_map_update | Kernel | 93794.43 | 93911.56 | 93841.95 | 34.61 |
| __bench_hash_map_update | Userspace | 62215.83 | 63703.60 | 63354.95 | 544.14 |
| __bench_per_cpu_array_map_delete | Kernel | 3517.28 | 3525.98 | 3519.52 | 2.42 |
| __bench_per_cpu_array_map_delete | Userspace | 16315.38 | 16560.32 | 16441.25 | 88.28 |
| __bench_per_cpu_array_map_lookup | Kernel | 4253.40 | 4276.26 | 4262.84 | 5.58 |
| __bench_per_cpu_array_map_lookup | Userspace | 27675.00 | 28353.96 | 27808.33 | 187.13 |
| __bench_per_cpu_array_map_update | Kernel | 16628.06 | 16727.37 | 16670.01 | 31.87 |
| __bench_per_cpu_array_map_update | Userspace | 75748.47 | 76099.09 | 75910.25 | 121.01 |
| __bench_per_cpu_hash_map_delete | Kernel | 150893.00 | 151048.70 | 150963.25 | 46.57 |
| __bench_per_cpu_hash_map_delete | Userspace | 984107.77 | 986202.29 | 985008.92 | 690.36 |
| __bench_per_cpu_hash_map_lookup | Kernel | 28910.26 | 29114.51 | 29042.05 | 63.97 |
| __bench_per_cpu_hash_map_lookup | Userspace | 141064.87 | 142045.34 | 141496.76 | 263.70 |
| __bench_per_cpu_hash_map_update | Kernel | 67822.23 | 67878.53 | 67857.34 | 19.76 |
| __bench_per_cpu_hash_map_update | Userspace | 220805.24 | 221948.41 | 221304.51 | 442.97 |
| __bench_read | Kernel | 18029.17 | 18111.93 | 18075.97 | 27.22 |
| __bench_read | Userspace | 4682.35 | 4687.40 | 4684.34 | 1.94 |
| __bench_uprobe | Kernel | 1224.62 | 1244.57 | 1232.44 | 6.72 |
| __bench_uprobe | Userspace | 316.84 | 320.78 | 318.60 | 1.41 |
| __bench_uprobe_uretprobe | Kernel | 1749.12 | 1765.24 | 1756.97 | 4.96 |
| __bench_uprobe_uretprobe | Userspace | 333.35 | 337.64 | 334.77 | 1.20 |
| __bench_uretprobe | Kernel | 1637.22 | 1657.03 | 1647.03 | 7.47 |
| __bench_uretprobe | Userspace | 317.40 | 352.67 | 325.38 | 13.08 |
| __bench_write | Kernel | 17622.60 | 20847.96 | 18009.94 | 946.70 |
| __bench_write | Userspace | 4396.50 | 4970.11 | 4674.18 | 243.07 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 161.03 | 412.54 | 189.44 | 74.43 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-09-10 13:34:37
- **Total duration:** 4751.95 seconds

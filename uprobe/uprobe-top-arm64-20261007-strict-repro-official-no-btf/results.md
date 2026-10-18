# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-07 16:10:27*

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
| __bench_uprobe | 1228.30 | 319.06 | 3.85x |
| __bench_uretprobe | 1646.23 | 332.24 | 4.95x |
| __bench_uprobe_uretprobe | 1752.60 | 334.66 | 5.24x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3681.98 | 3691.07 | 3686.17 | 3.68 |
| __bench_array_map_delete | Userspace | 16332.37 | 16479.57 | 16394.02 | 48.34 |
| __bench_array_map_lookup | Kernel | 3776.77 | 3786.87 | 3779.64 | 3.35 |
| __bench_array_map_lookup | Userspace | 15805.04 | 16079.64 | 15937.20 | 80.58 |
| __bench_array_map_update | Kernel | 13179.51 | 13241.89 | 13208.83 | 17.02 |
| __bench_array_map_update | Userspace | 23337.94 | 23719.40 | 23509.55 | 130.56 |
| __bench_hash_map_delete | Kernel | 107848.32 | 107945.97 | 107892.16 | 33.77 |
| __bench_hash_map_delete | Userspace | 41871.21 | 43773.05 | 42638.57 | 658.91 |
| __bench_hash_map_lookup | Kernel | 28525.71 | 28740.29 | 28636.59 | 74.70 |
| __bench_hash_map_lookup | Userspace | 57699.60 | 59770.79 | 58472.54 | 902.98 |
| __bench_hash_map_update | Kernel | 93511.41 | 93712.49 | 93608.69 | 51.96 |
| __bench_hash_map_update | Userspace | 62157.48 | 64672.76 | 63182.38 | 805.25 |
| __bench_per_cpu_array_map_delete | Kernel | 3515.79 | 3516.76 | 3516.31 | 0.38 |
| __bench_per_cpu_array_map_delete | Userspace | 16317.21 | 16573.26 | 16437.65 | 84.63 |
| __bench_per_cpu_array_map_lookup | Kernel | 4257.79 | 4264.00 | 4260.61 | 2.36 |
| __bench_per_cpu_array_map_lookup | Userspace | 27605.52 | 27822.12 | 27727.95 | 55.16 |
| __bench_per_cpu_array_map_update | Kernel | 16654.53 | 16694.91 | 16675.81 | 12.52 |
| __bench_per_cpu_array_map_update | Userspace | 75792.86 | 76061.17 | 75928.93 | 85.51 |
| __bench_per_cpu_hash_map_delete | Kernel | 151642.19 | 151877.81 | 151725.41 | 67.38 |
| __bench_per_cpu_hash_map_delete | Userspace | 984177.86 | 996848.24 | 986684.05 | 3773.81 |
| __bench_per_cpu_hash_map_lookup | Kernel | 28337.12 | 28474.06 | 28408.27 | 39.35 |
| __bench_per_cpu_hash_map_lookup | Userspace | 140597.69 | 141631.20 | 141245.11 | 361.49 |
| __bench_per_cpu_hash_map_update | Kernel | 67571.03 | 67760.39 | 67704.12 | 50.22 |
| __bench_per_cpu_hash_map_update | Userspace | 219965.87 | 221769.60 | 220961.13 | 581.71 |
| __bench_read | Kernel | 18006.84 | 18201.38 | 18096.57 | 45.28 |
| __bench_read | Userspace | 4680.67 | 4688.03 | 4683.67 | 1.81 |
| __bench_uprobe | Kernel | 1222.50 | 1236.61 | 1228.30 | 4.59 |
| __bench_uprobe | Userspace | 317.02 | 321.12 | 319.06 | 1.46 |
| __bench_uprobe_uretprobe | Kernel | 1746.72 | 1757.58 | 1752.60 | 4.18 |
| __bench_uprobe_uretprobe | Userspace | 333.70 | 335.33 | 334.66 | 0.45 |
| __bench_uretprobe | Kernel | 1636.89 | 1658.09 | 1646.23 | 7.24 |
| __bench_uretprobe | Userspace | 317.46 | 353.15 | 332.24 | 15.86 |
| __bench_write | Kernel | 17619.43 | 17784.71 | 17675.62 | 48.26 |
| __bench_write | Userspace | 4393.49 | 4970.18 | 4550.68 | 223.90 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 160.82 | 170.50 | 166.06 | 3.30 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-07 16:10:27
- **Total duration:** 4749.58 seconds

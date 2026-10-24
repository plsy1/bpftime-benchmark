# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-13 18:46:29*

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
| __bench_uprobe | 1228.27 | 317.93 | 3.86x |
| __bench_uretprobe | 1643.93 | 318.40 | 5.16x |
| __bench_uprobe_uretprobe | 1756.85 | 334.51 | 5.25x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3676.76 | 3688.27 | 3680.94 | 4.06 |
| __bench_array_map_delete | Userspace | 16199.46 | 16281.88 | 16236.46 | 25.86 |
| __bench_array_map_lookup | Kernel | 3738.98 | 3757.14 | 3743.70 | 5.51 |
| __bench_array_map_lookup | Userspace | 15840.47 | 16076.69 | 15953.39 | 78.16 |
| __bench_array_map_update | Kernel | 13066.44 | 13113.18 | 13091.61 | 13.58 |
| __bench_array_map_update | Userspace | 23651.80 | 24196.08 | 23771.27 | 156.77 |
| __bench_hash_map_delete | Kernel | 107847.50 | 107959.31 | 107918.22 | 32.07 |
| __bench_hash_map_delete | Userspace | 41506.40 | 43355.79 | 42267.31 | 745.64 |
| __bench_hash_map_lookup | Kernel | 28151.78 | 28356.21 | 28257.12 | 56.82 |
| __bench_hash_map_lookup | Userspace | 57975.52 | 60298.87 | 58833.58 | 962.22 |
| __bench_hash_map_update | Kernel | 94067.18 | 94216.24 | 94148.56 | 34.82 |
| __bench_hash_map_update | Userspace | 62050.84 | 63894.55 | 62992.11 | 764.55 |
| __bench_per_cpu_array_map_delete | Kernel | 3683.60 | 3700.03 | 3689.06 | 6.30 |
| __bench_per_cpu_array_map_delete | Userspace | 16168.19 | 16351.09 | 16250.82 | 67.64 |
| __bench_per_cpu_array_map_lookup | Kernel | 4397.45 | 4416.50 | 4403.71 | 6.28 |
| __bench_per_cpu_array_map_lookup | Userspace | 21312.31 | 21489.82 | 21380.63 | 59.70 |
| __bench_per_cpu_array_map_update | Kernel | 16562.44 | 16618.58 | 16588.48 | 17.13 |
| __bench_per_cpu_array_map_update | Userspace | 27327.06 | 27421.59 | 27376.60 | 33.61 |
| __bench_per_cpu_hash_map_delete | Kernel | 151543.24 | 151658.37 | 151591.70 | 30.11 |
| __bench_per_cpu_hash_map_delete | Userspace | 920625.56 | 936743.79 | 923316.63 | 4567.63 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29319.46 | 29455.91 | 29388.31 | 46.00 |
| __bench_per_cpu_hash_map_lookup | Userspace | 67673.33 | 68352.07 | 68011.14 | 209.05 |
| __bench_per_cpu_hash_map_update | Kernel | 67724.55 | 67791.78 | 67760.12 | 21.17 |
| __bench_per_cpu_hash_map_update | Userspace | 78963.45 | 79842.14 | 79502.18 | 281.56 |
| __bench_read | Kernel | 18056.25 | 18150.09 | 18114.54 | 22.97 |
| __bench_read | Userspace | 4690.34 | 4696.01 | 4692.94 | 1.77 |
| __bench_uprobe | Kernel | 1225.22 | 1238.92 | 1228.27 | 3.87 |
| __bench_uprobe | Userspace | 315.83 | 320.03 | 317.93 | 1.40 |
| __bench_uprobe_uretprobe | Kernel | 1745.69 | 1775.41 | 1756.85 | 8.05 |
| __bench_uprobe_uretprobe | Userspace | 333.39 | 335.70 | 334.51 | 0.81 |
| __bench_uretprobe | Kernel | 1638.30 | 1661.14 | 1643.93 | 6.71 |
| __bench_uretprobe | Userspace | 316.34 | 320.99 | 318.40 | 1.49 |
| __bench_write | Kernel | 17565.08 | 17807.24 | 17652.80 | 65.65 |
| __bench_write | Userspace | 4406.47 | 4977.60 | 4681.20 | 182.92 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 156.36 | 178.88 | 160.39 | 6.29 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-13 18:46:29
- **Total duration:** 4272.02 seconds

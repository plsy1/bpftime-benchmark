# BPFtime Uprobe Benchmark Results

*Generated on 2026-11-03 06:07:15*

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
| __bench_uprobe | 1250.28 | 201.36 | 6.21x |
| __bench_uretprobe | 1668.04 | 270.23 | 6.17x |
| __bench_uprobe_uretprobe | 1781.16 | 334.30 | 5.33x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3742.61 | 3759.39 | 3750.09 | 6.07 |
| __bench_array_map_delete | Userspace | 20855.77 | 21070.61 | 20988.89 | 86.94 |
| __bench_array_map_lookup | Kernel | 3817.23 | 3845.98 | 3831.48 | 9.47 |
| __bench_array_map_lookup | Userspace | 1173.89 | 1181.97 | 1178.68 | 2.29 |
| __bench_array_map_update | Kernel | 13123.96 | 13154.71 | 13142.67 | 7.54 |
| __bench_array_map_update | Userspace | 11483.94 | 11556.69 | 11528.68 | 23.53 |
| __bench_hash_map_delete | Kernel | 107658.00 | 107835.32 | 107769.52 | 49.97 |
| __bench_hash_map_delete | Userspace | 35053.42 | 37727.07 | 36438.47 | 749.45 |
| __bench_hash_map_lookup | Kernel | 28510.55 | 28699.66 | 28609.03 | 59.57 |
| __bench_hash_map_lookup | Userspace | 16030.44 | 16098.54 | 16059.48 | 20.01 |
| __bench_hash_map_update | Kernel | 95750.82 | 95927.50 | 95829.89 | 51.78 |
| __bench_hash_map_update | Userspace | 26291.02 | 27021.89 | 26482.20 | 201.43 |
| __bench_per_cpu_array_map_delete | Kernel | 3744.89 | 3847.00 | 3765.78 | 30.06 |
| __bench_per_cpu_array_map_delete | Userspace | 20890.98 | 21116.49 | 21018.20 | 78.40 |
| __bench_per_cpu_array_map_lookup | Kernel | 4518.45 | 4648.02 | 4538.53 | 37.49 |
| __bench_per_cpu_array_map_lookup | Userspace | 4098.75 | 4130.21 | 4114.72 | 9.64 |
| __bench_per_cpu_array_map_update | Kernel | 16607.88 | 16675.34 | 16635.41 | 20.41 |
| __bench_per_cpu_array_map_update | Userspace | 14765.60 | 14975.57 | 14837.29 | 62.49 |
| __bench_per_cpu_hash_map_delete | Kernel | 151390.10 | 151919.05 | 151688.92 | 173.08 |
| __bench_per_cpu_hash_map_delete | Userspace | 64573.55 | 65501.55 | 65198.15 | 330.15 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29627.32 | 29918.60 | 29780.47 | 89.82 |
| __bench_per_cpu_hash_map_lookup | Userspace | 23163.76 | 23577.86 | 23374.67 | 182.12 |
| __bench_per_cpu_hash_map_update | Kernel | 67901.59 | 68142.35 | 67980.24 | 72.73 |
| __bench_per_cpu_hash_map_update | Userspace | 33929.43 | 35556.32 | 34300.56 | 554.64 |
| __bench_read | Kernel | 17573.65 | 17694.09 | 17631.38 | 38.86 |
| __bench_read | Userspace | 4851.86 | 4862.22 | 4855.48 | 3.25 |
| __bench_uprobe | Kernel | 1239.34 | 1269.92 | 1250.28 | 11.55 |
| __bench_uprobe | Userspace | 198.57 | 204.08 | 201.36 | 1.66 |
| __bench_uprobe_uretprobe | Kernel | 1770.68 | 1806.55 | 1781.16 | 11.39 |
| __bench_uprobe_uretprobe | Userspace | 325.42 | 350.24 | 334.30 | 8.83 |
| __bench_uretprobe | Kernel | 1656.66 | 1695.84 | 1668.04 | 11.02 |
| __bench_uretprobe | Userspace | 268.08 | 272.25 | 270.23 | 1.49 |
| __bench_write | Kernel | 17735.78 | 17956.42 | 17844.52 | 62.62 |
| __bench_write | Userspace | 4275.12 | 4856.23 | 4520.37 | 271.48 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 155.21 | 174.74 | 158.33 | 5.68 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-11-03 06:07:15
- **Total duration:** 1285.17 seconds

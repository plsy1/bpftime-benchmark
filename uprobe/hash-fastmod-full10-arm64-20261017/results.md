# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-18 00:24:13*

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
| __bench_uprobe | 1226.43 | 197.02 | 6.22x |
| __bench_uretprobe | 1641.09 | 267.02 | 6.15x |
| __bench_uprobe_uretprobe | 1751.44 | 319.67 | 5.48x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3682.05 | 3695.12 | 3686.28 | 4.36 |
| __bench_array_map_delete | Userspace | 19930.31 | 20190.57 | 20082.36 | 99.82 |
| __bench_array_map_lookup | Kernel | 3802.38 | 3810.59 | 3804.63 | 2.38 |
| __bench_array_map_lookup | Userspace | 7946.28 | 8520.72 | 8012.49 | 169.58 |
| __bench_array_map_update | Kernel | 13189.11 | 13249.08 | 13214.81 | 18.44 |
| __bench_array_map_update | Userspace | 11514.29 | 11575.29 | 11548.28 | 22.12 |
| __bench_hash_map_delete | Kernel | 107877.72 | 108072.83 | 107942.13 | 58.22 |
| __bench_hash_map_delete | Userspace | 36328.69 | 36767.89 | 36530.14 | 126.19 |
| __bench_hash_map_lookup | Kernel | 28032.18 | 28390.07 | 28219.82 | 111.83 |
| __bench_hash_map_lookup | Userspace | 27935.21 | 28681.85 | 28163.50 | 207.84 |
| __bench_hash_map_update | Kernel | 93736.82 | 93868.03 | 93778.61 | 34.03 |
| __bench_hash_map_update | Userspace | 33009.05 | 35983.31 | 34583.19 | 1305.11 |
| __bench_per_cpu_array_map_delete | Kernel | 3682.63 | 3691.53 | 3686.40 | 3.36 |
| __bench_per_cpu_array_map_delete | Userspace | 20001.24 | 20211.66 | 20092.68 | 55.40 |
| __bench_per_cpu_array_map_lookup | Kernel | 4397.75 | 4408.52 | 4400.67 | 3.61 |
| __bench_per_cpu_array_map_lookup | Userspace | 11590.16 | 11701.05 | 11627.55 | 36.21 |
| __bench_per_cpu_array_map_update | Kernel | 16571.12 | 16633.34 | 16600.54 | 19.60 |
| __bench_per_cpu_array_map_update | Userspace | 14217.85 | 14429.35 | 14316.12 | 64.65 |
| __bench_per_cpu_hash_map_delete | Kernel | 152296.11 | 152449.24 | 152371.61 | 54.24 |
| __bench_per_cpu_hash_map_delete | Userspace | 98629.02 | 99944.18 | 99052.51 | 443.34 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29326.91 | 29487.43 | 29396.29 | 42.48 |
| __bench_per_cpu_hash_map_lookup | Userspace | 41539.60 | 41720.31 | 41619.70 | 53.86 |
| __bench_per_cpu_hash_map_update | Kernel | 68265.37 | 68581.73 | 68389.82 | 85.30 |
| __bench_per_cpu_hash_map_update | Userspace | 44290.07 | 44442.52 | 44373.42 | 42.65 |
| __bench_read | Kernel | 17955.95 | 19323.37 | 18198.50 | 380.10 |
| __bench_read | Userspace | 4582.65 | 4591.97 | 4586.96 | 3.09 |
| __bench_uprobe | Kernel | 1223.49 | 1233.00 | 1226.43 | 3.66 |
| __bench_uprobe | Userspace | 195.21 | 200.31 | 197.02 | 1.50 |
| __bench_uprobe_uretprobe | Kernel | 1748.21 | 1757.57 | 1751.44 | 3.05 |
| __bench_uprobe_uretprobe | Userspace | 318.71 | 320.72 | 319.67 | 0.75 |
| __bench_uretprobe | Kernel | 1635.59 | 1653.96 | 1641.09 | 5.56 |
| __bench_uretprobe | Userspace | 264.97 | 269.26 | 267.02 | 1.37 |
| __bench_write | Kernel | 17663.65 | 17752.34 | 17718.81 | 32.52 |
| __bench_write | Userspace | 4269.70 | 4275.42 | 4272.88 | 1.43 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 155.64 | 164.76 | 159.46 | 3.03 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-18 00:24:13
- **Total duration:** 1510.97 seconds

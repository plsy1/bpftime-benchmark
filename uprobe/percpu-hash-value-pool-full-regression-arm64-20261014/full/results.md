# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-14 12:24:20*

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
| __bench_uprobe | 1229.06 | 318.96 | 3.85x |
| __bench_uretprobe | 1643.08 | 317.23 | 5.18x |
| __bench_uprobe_uretprobe | 1756.17 | 334.28 | 5.25x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3682.52 | 3701.64 | 3688.19 | 5.60 |
| __bench_array_map_delete | Userspace | 16196.10 | 16411.82 | 16297.70 | 75.02 |
| __bench_array_map_lookup | Kernel | 3806.33 | 3820.23 | 3810.31 | 5.02 |
| __bench_array_map_lookup | Userspace | 15839.06 | 16042.89 | 15928.92 | 55.36 |
| __bench_array_map_update | Kernel | 13073.07 | 13110.83 | 13091.42 | 14.65 |
| __bench_array_map_update | Userspace | 23311.87 | 23813.92 | 23475.62 | 143.31 |
| __bench_hash_map_delete | Kernel | 107924.95 | 108024.74 | 107969.58 | 31.80 |
| __bench_hash_map_delete | Userspace | 41528.51 | 43621.20 | 42327.04 | 746.74 |
| __bench_hash_map_lookup | Kernel | 28530.35 | 28778.39 | 28666.74 | 70.48 |
| __bench_hash_map_lookup | Userspace | 54939.27 | 57492.56 | 55845.20 | 959.61 |
| __bench_hash_map_update | Kernel | 93559.46 | 94077.28 | 93798.09 | 208.21 |
| __bench_hash_map_update | Userspace | 61565.60 | 62214.24 | 61894.21 | 171.52 |
| __bench_per_cpu_array_map_delete | Kernel | 3683.48 | 3695.07 | 3686.16 | 3.23 |
| __bench_per_cpu_array_map_delete | Userspace | 16120.39 | 16462.16 | 16249.64 | 115.90 |
| __bench_per_cpu_array_map_lookup | Kernel | 4397.54 | 4407.06 | 4402.29 | 3.55 |
| __bench_per_cpu_array_map_lookup | Userspace | 21393.41 | 21632.77 | 21481.07 | 66.41 |
| __bench_per_cpu_array_map_update | Kernel | 16606.67 | 16657.42 | 16629.04 | 17.45 |
| __bench_per_cpu_array_map_update | Userspace | 27304.27 | 27480.96 | 27369.25 | 47.77 |
| __bench_per_cpu_hash_map_delete | Kernel | 151625.06 | 153380.52 | 152351.54 | 806.98 |
| __bench_per_cpu_hash_map_delete | Userspace | 362972.73 | 363762.26 | 363364.38 | 286.65 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29671.84 | 29843.59 | 29740.74 | 48.53 |
| __bench_per_cpu_hash_map_lookup | Userspace | 61632.95 | 61984.63 | 61758.02 | 108.41 |
| __bench_per_cpu_hash_map_update | Kernel | 67894.96 | 67979.76 | 67948.49 | 29.35 |
| __bench_per_cpu_hash_map_update | Userspace | 69959.89 | 71090.19 | 70281.26 | 322.04 |
| __bench_read | Kernel | 18062.73 | 18183.21 | 18102.40 | 30.60 |
| __bench_read | Userspace | 4680.21 | 4688.81 | 4682.76 | 2.38 |
| __bench_uprobe | Kernel | 1225.11 | 1235.93 | 1229.06 | 2.83 |
| __bench_uprobe | Userspace | 316.53 | 321.28 | 318.96 | 1.49 |
| __bench_uprobe_uretprobe | Kernel | 1747.04 | 1769.22 | 1756.17 | 6.08 |
| __bench_uprobe_uretprobe | Userspace | 332.82 | 335.93 | 334.28 | 1.11 |
| __bench_uretprobe | Kernel | 1636.68 | 1653.08 | 1643.08 | 5.14 |
| __bench_uretprobe | Userspace | 315.78 | 319.49 | 317.23 | 1.32 |
| __bench_write | Kernel | 17617.21 | 17806.34 | 17716.52 | 46.58 |
| __bench_write | Userspace | 4394.86 | 4970.67 | 4634.59 | 214.51 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 159.32 | 189.09 | 163.57 | 8.61 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-14 12:24:20
- **Total duration:** 2176.31 seconds

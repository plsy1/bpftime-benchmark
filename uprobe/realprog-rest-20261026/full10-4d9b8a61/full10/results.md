# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-26 01:17:34*

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

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Userspace | 20828.92 | 20997.24 | 20884.83 | 51.31 |
| __bench_array_map_lookup | Userspace | 1172.01 | 1177.20 | 1174.61 | 1.44 |
| __bench_array_map_update | Userspace | 11493.23 | 11543.97 | 11522.88 | 15.79 |
| __bench_hash_map_delete | Userspace | 27397.68 | 28593.96 | 28106.35 | 542.19 |
| __bench_hash_map_lookup | Userspace | 22976.61 | 23644.31 | 23192.76 | 236.26 |
| __bench_hash_map_update | Userspace | 27258.42 | 28669.99 | 28062.82 | 652.11 |
| __bench_per_cpu_array_map_delete | Userspace | 20904.54 | 21093.23 | 20954.19 | 69.49 |
| __bench_per_cpu_array_map_lookup | Userspace | 4096.63 | 4126.04 | 4110.97 | 8.64 |
| __bench_per_cpu_array_map_update | Userspace | 14293.18 | 14470.65 | 14361.58 | 54.24 |
| __bench_per_cpu_hash_map_delete | Userspace | 99323.62 | 99642.85 | 99447.50 | 88.59 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27779.86 | 27904.49 | 27850.36 | 40.87 |
| __bench_per_cpu_hash_map_update | Userspace | 44480.45 | 44558.19 | 44521.92 | 25.31 |
| __bench_read | Userspace | 6011.39 | 6027.61 | 6018.31 | 5.08 |
| __bench_uprobe | Userspace | 198.52 | 204.73 | 201.22 | 1.56 |
| __bench_uprobe_uretprobe | Userspace | 327.17 | 332.80 | 329.06 | 1.70 |
| __bench_uretprobe | Userspace | 270.50 | 277.34 | 273.06 | 1.85 |
| __bench_write | Userspace | 5432.78 | 5438.14 | 5434.81 | 1.45 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 153.47 | 178.77 | 157.37 | 7.29 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-26 01:17:34
- **Total duration:** 512.30 seconds

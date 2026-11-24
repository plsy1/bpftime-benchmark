# BPFtime Uprobe Benchmark Results

*Generated on 2026-11-24 04:32:11*

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
| __bench_uprobe | 1241.49 | 196.36 | 6.32x |
| __bench_uretprobe | 1658.88 | 265.45 | 6.25x |
| __bench_uprobe_uretprobe | 1774.31 | 319.71 | 5.55x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3747.50 | 3758.71 | 3752.57 | 3.23 |
| __bench_array_map_delete | Userspace | 20277.42 | 20451.92 | 20382.57 | 59.09 |
| __bench_array_map_lookup | Kernel | 3788.24 | 3832.52 | 3799.68 | 11.83 |
| __bench_array_map_lookup | Userspace | 19984.36 | 20116.94 | 20045.73 | 42.65 |
| __bench_array_map_update | Kernel | 13082.52 | 13165.66 | 13128.33 | 22.39 |
| __bench_array_map_update | Userspace | 25345.17 | 25450.66 | 25401.34 | 27.25 |
| __bench_hash_map_delete | Kernel | 109443.63 | 109645.12 | 109581.96 | 57.99 |
| __bench_hash_map_delete | Userspace | 47476.66 | 47918.16 | 47665.42 | 145.89 |
| __bench_hash_map_lookup | Kernel | 28995.92 | 29351.73 | 29177.47 | 110.05 |
| __bench_hash_map_lookup | Userspace | 51844.34 | 52935.92 | 52306.37 | 405.08 |
| __bench_hash_map_update | Kernel | 96026.43 | 96186.74 | 96109.94 | 52.28 |
| __bench_hash_map_update | Userspace | 61653.84 | 62542.36 | 62046.97 | 279.40 |
| __bench_per_cpu_array_map_delete | Kernel | 3579.37 | 3598.33 | 3585.40 | 5.07 |
| __bench_per_cpu_array_map_delete | Userspace | 20318.37 | 20495.27 | 20423.50 | 58.55 |
| __bench_per_cpu_array_map_lookup | Kernel | 4297.63 | 4311.25 | 4304.33 | 3.84 |
| __bench_per_cpu_array_map_lookup | Userspace | 28585.18 | 29055.12 | 28805.62 | 157.57 |
| __bench_per_cpu_array_map_update | Kernel | 16554.51 | 16607.89 | 16586.55 | 15.65 |
| __bench_per_cpu_array_map_update | Userspace | 69752.88 | 70576.95 | 70434.17 | 231.44 |
| __bench_per_cpu_hash_map_delete | Kernel | 150731.58 | 150848.86 | 150794.33 | 32.77 |
| __bench_per_cpu_hash_map_delete | Userspace | 257150.29 | 257581.83 | 257297.37 | 130.59 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29479.63 | 29860.87 | 29640.53 | 91.91 |
| __bench_per_cpu_hash_map_lookup | Userspace | 128660.42 | 128936.47 | 128826.46 | 77.12 |
| __bench_per_cpu_hash_map_update | Kernel | 67797.33 | 67935.80 | 67857.24 | 43.70 |
| __bench_per_cpu_hash_map_update | Userspace | 212731.64 | 213714.69 | 213361.72 | 326.11 |
| __bench_read | Kernel | 17583.45 | 17688.05 | 17620.25 | 30.41 |
| __bench_read | Userspace | 851993.60 | 884677.37 | 863163.78 | 8956.51 |
| __bench_uprobe | Kernel | 1237.82 | 1248.46 | 1241.49 | 2.83 |
| __bench_uprobe | Userspace | 194.60 | 197.74 | 196.36 | 1.08 |
| __bench_uprobe_uretprobe | Kernel | 1770.29 | 1782.29 | 1774.31 | 3.75 |
| __bench_uprobe_uretprobe | Userspace | 318.40 | 321.02 | 319.71 | 0.78 |
| __bench_uretprobe | Kernel | 1654.23 | 1666.33 | 1658.88 | 4.54 |
| __bench_uretprobe | Userspace | 263.38 | 267.44 | 265.45 | 1.08 |
| __bench_write | Kernel | 17668.64 | 17770.32 | 17736.83 | 30.33 |
| __bench_write | Userspace | 857860.32 | 882767.28 | 866065.83 | 8087.37 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 154.57 | 177.06 | 158.86 | 6.29 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-11-24 04:32:11
- **Total duration:** 3983.50 seconds

# BPFtime Uprobe Benchmark Results

*Generated on 2026-11-23 20:15:29*

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
| __bench_uprobe | 1243.10 | 211.12 | 5.89x |
| __bench_uretprobe | 1663.01 | 303.40 | 5.48x |
| __bench_uprobe_uretprobe | 1777.27 | 346.86 | 5.12x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3562.38 | 3585.25 | 3575.15 | 6.34 |
| __bench_array_map_delete | Userspace | 20289.96 | 20572.57 | 20393.86 | 109.77 |
| __bench_array_map_lookup | Kernel | 3737.42 | 3828.10 | 3800.16 | 23.46 |
| __bench_array_map_lookup | Userspace | 1183.03 | 1188.05 | 1185.22 | 1.63 |
| __bench_array_map_update | Kernel | 13074.46 | 13100.44 | 13088.22 | 8.88 |
| __bench_array_map_update | Userspace | 11505.87 | 11575.89 | 11535.80 | 20.64 |
| __bench_hash_map_delete | Kernel | 108181.17 | 108306.73 | 108235.70 | 40.26 |
| __bench_hash_map_delete | Userspace | 34487.91 | 35634.84 | 35101.01 | 438.96 |
| __bench_hash_map_lookup | Kernel | 28713.51 | 29032.42 | 28848.14 | 98.04 |
| __bench_hash_map_lookup | Userspace | 17744.15 | 17870.89 | 17810.39 | 40.09 |
| __bench_hash_map_update | Kernel | 94656.30 | 94936.66 | 94806.59 | 83.69 |
| __bench_hash_map_update | Userspace | 31200.67 | 32100.72 | 31582.00 | 358.47 |
| __bench_per_cpu_array_map_delete | Kernel | 3573.39 | 3581.68 | 3576.50 | 3.17 |
| __bench_per_cpu_array_map_delete | Userspace | 20280.93 | 20579.00 | 20407.45 | 109.07 |
| __bench_per_cpu_array_map_lookup | Kernel | 4296.14 | 4307.16 | 4299.92 | 2.74 |
| __bench_per_cpu_array_map_lookup | Userspace | 4116.25 | 4134.71 | 4123.90 | 5.51 |
| __bench_per_cpu_array_map_update | Kernel | 16720.49 | 16756.63 | 16733.41 | 11.10 |
| __bench_per_cpu_array_map_update | Userspace | 14986.58 | 15109.14 | 15033.89 | 39.15 |
| __bench_per_cpu_hash_map_delete | Kernel | 148691.22 | 148812.72 | 148741.03 | 39.72 |
| __bench_per_cpu_hash_map_delete | Userspace | 65954.22 | 68378.99 | 66899.58 | 895.79 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29241.46 | 29417.87 | 29310.66 | 45.94 |
| __bench_per_cpu_hash_map_lookup | Userspace | 24102.06 | 24162.40 | 24124.61 | 19.51 |
| __bench_per_cpu_hash_map_update | Kernel | 68101.57 | 68214.03 | 68148.72 | 32.15 |
| __bench_per_cpu_hash_map_update | Userspace | 33839.66 | 34912.56 | 34215.03 | 408.91 |
| __bench_read | Kernel | 17578.80 | 17623.73 | 17603.47 | 15.20 |
| __bench_read | Userspace | 4581.37 | 4591.63 | 4587.00 | 3.22 |
| __bench_uprobe | Kernel | 1239.93 | 1255.36 | 1243.10 | 5.07 |
| __bench_uprobe | Userspace | 209.91 | 213.09 | 211.12 | 1.14 |
| __bench_uprobe_uretprobe | Kernel | 1768.87 | 1794.54 | 1777.27 | 8.88 |
| __bench_uprobe_uretprobe | Userspace | 345.62 | 349.40 | 346.86 | 1.14 |
| __bench_uretprobe | Kernel | 1656.42 | 1676.96 | 1663.01 | 5.99 |
| __bench_uretprobe | Userspace | 302.41 | 304.62 | 303.40 | 0.61 |
| __bench_write | Kernel | 17727.83 | 22509.53 | 18293.89 | 1405.88 |
| __bench_write | Userspace | 4284.48 | 4289.07 | 4286.65 | 1.33 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.59 | 176.11 | 159.10 | 7.27 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-11-23 20:15:29
- **Total duration:** 1284.21 seconds

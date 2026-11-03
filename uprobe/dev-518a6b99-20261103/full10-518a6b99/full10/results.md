# BPFtime Uprobe Benchmark Results

*Generated on 2026-11-03 20:20:42*

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
| __bench_uprobe | 1242.87 | 198.95 | 6.25x |
| __bench_uretprobe | 1670.27 | 266.91 | 6.26x |
| __bench_uprobe_uretprobe | 1784.71 | 324.79 | 5.49x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3741.01 | 3751.75 | 3745.53 | 3.60 |
| __bench_array_map_delete | Userspace | 20780.19 | 21014.59 | 20926.73 | 75.82 |
| __bench_array_map_lookup | Kernel | 3750.53 | 3764.41 | 3757.42 | 3.26 |
| __bench_array_map_lookup | Userspace | 1173.16 | 1177.38 | 1175.37 | 1.09 |
| __bench_array_map_update | Kernel | 13113.86 | 13167.42 | 13135.64 | 16.26 |
| __bench_array_map_update | Userspace | 11504.29 | 11763.41 | 11549.51 | 72.58 |
| __bench_hash_map_delete | Kernel | 108349.84 | 108463.80 | 108407.09 | 36.67 |
| __bench_hash_map_delete | Userspace | 34200.13 | 37131.40 | 35332.09 | 1071.60 |
| __bench_hash_map_lookup | Kernel | 28482.88 | 28777.27 | 28645.15 | 98.57 |
| __bench_hash_map_lookup | Userspace | 17879.08 | 17949.48 | 17914.31 | 18.57 |
| __bench_hash_map_update | Kernel | 95161.30 | 95383.22 | 95254.59 | 61.48 |
| __bench_hash_map_update | Userspace | 33895.15 | 34802.47 | 34088.55 | 269.91 |
| __bench_per_cpu_array_map_delete | Kernel | 3744.89 | 3766.44 | 3753.12 | 6.50 |
| __bench_per_cpu_array_map_delete | Userspace | 20850.90 | 21252.87 | 21010.40 | 126.45 |
| __bench_per_cpu_array_map_lookup | Kernel | 4493.04 | 4540.04 | 4519.90 | 11.38 |
| __bench_per_cpu_array_map_lookup | Userspace | 4102.25 | 4131.49 | 4118.04 | 10.81 |
| __bench_per_cpu_array_map_update | Kernel | 16625.30 | 16675.09 | 16645.16 | 16.56 |
| __bench_per_cpu_array_map_update | Userspace | 14750.30 | 15250.80 | 14948.09 | 126.68 |
| __bench_per_cpu_hash_map_delete | Kernel | 150447.16 | 150551.32 | 150480.49 | 28.35 |
| __bench_per_cpu_hash_map_delete | Userspace | 63739.59 | 65692.41 | 64500.38 | 525.63 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29338.08 | 29524.30 | 29412.17 | 46.67 |
| __bench_per_cpu_hash_map_lookup | Userspace | 24675.80 | 24794.99 | 24738.07 | 34.06 |
| __bench_per_cpu_hash_map_update | Kernel | 68025.11 | 68115.51 | 68078.59 | 21.99 |
| __bench_per_cpu_hash_map_update | Userspace | 33909.18 | 35242.88 | 34821.36 | 440.04 |
| __bench_read | Kernel | 17556.35 | 17625.12 | 17596.50 | 20.04 |
| __bench_read | Userspace | 4852.06 | 4854.73 | 4853.35 | 0.82 |
| __bench_uprobe | Kernel | 1233.94 | 1257.66 | 1242.87 | 7.53 |
| __bench_uprobe | Userspace | 196.15 | 200.73 | 198.95 | 1.59 |
| __bench_uprobe_uretprobe | Kernel | 1769.69 | 1807.48 | 1784.71 | 15.24 |
| __bench_uprobe_uretprobe | Userspace | 323.47 | 328.17 | 324.79 | 1.28 |
| __bench_uretprobe | Kernel | 1655.97 | 1691.70 | 1670.27 | 11.86 |
| __bench_uretprobe | Userspace | 265.60 | 269.02 | 266.91 | 0.94 |
| __bench_write | Kernel | 17791.03 | 17844.04 | 17818.65 | 16.03 |
| __bench_write | Userspace | 4272.18 | 4849.24 | 4456.30 | 222.40 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 153.17 | 181.97 | 158.94 | 8.17 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-11-03 20:20:42
- **Total duration:** 1296.98 seconds

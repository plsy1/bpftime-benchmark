# BPFtime Uprobe Benchmark Results

*Generated on 2026-09-09 22:00:24*

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
| __bench_uprobe | 1226.15 | 366.02 | 3.35x |
| __bench_uretprobe | 1642.76 | 363.74 | 4.52x |
| __bench_uprobe_uretprobe | 1759.84 | 420.27 | 4.19x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3690.45 | 3718.94 | 3697.18 | 8.82 |
| __bench_array_map_delete | Userspace | 17596.79 | 17755.71 | 17705.32 | 46.72 |
| __bench_array_map_lookup | Kernel | 3778.95 | 3801.73 | 3788.68 | 7.92 |
| __bench_array_map_lookup | Userspace | 16885.09 | 17179.46 | 17037.89 | 86.27 |
| __bench_array_map_update | Kernel | 13068.41 | 13140.10 | 13099.42 | 22.32 |
| __bench_array_map_update | Userspace | 21721.97 | 22564.27 | 21937.36 | 277.08 |
| __bench_hash_map_delete | Kernel | 108093.39 | 108715.33 | 108278.32 | 184.48 |
| __bench_hash_map_delete | Userspace | 45180.20 | 46509.58 | 45817.18 | 544.98 |
| __bench_hash_map_lookup | Kernel | 28257.53 | 28523.77 | 28383.58 | 87.96 |
| __bench_hash_map_lookup | Userspace | 64208.27 | 65334.65 | 64775.04 | 398.86 |
| __bench_hash_map_update | Kernel | 94474.47 | 95002.03 | 94660.13 | 165.04 |
| __bench_hash_map_update | Userspace | 56877.42 | 57606.97 | 57139.14 | 214.18 |
| __bench_per_cpu_array_map_delete | Kernel | 3511.83 | 3540.00 | 3521.08 | 8.27 |
| __bench_per_cpu_array_map_delete | Userspace | 21077.24 | 21316.42 | 21197.50 | 79.89 |
| __bench_per_cpu_array_map_lookup | Kernel | 4229.65 | 4248.17 | 4236.91 | 6.99 |
| __bench_per_cpu_array_map_lookup | Userspace | 22368.38 | 22876.66 | 22556.11 | 133.76 |
| __bench_per_cpu_array_map_update | Kernel | 16635.71 | 16724.52 | 16673.94 | 28.67 |
| __bench_per_cpu_array_map_update | Userspace | 26635.86 | 26808.35 | 26740.36 | 54.85 |
| __bench_per_cpu_hash_map_delete | Kernel | 151979.01 | 152565.14 | 152261.32 | 207.91 |
| __bench_per_cpu_hash_map_delete | Userspace | 874141.61 | 877018.57 | 875553.00 | 815.36 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29035.45 | 29305.71 | 29187.07 | 77.73 |
| __bench_per_cpu_hash_map_lookup | Userspace | 143212.99 | 144216.90 | 143807.38 | 303.37 |
| __bench_per_cpu_hash_map_update | Kernel | 67866.09 | 68211.22 | 67983.01 | 109.77 |
| __bench_per_cpu_hash_map_update | Userspace | 212925.57 | 214281.91 | 213618.04 | 444.32 |
| __bench_read | Kernel | 18004.77 | 18228.13 | 18129.43 | 60.09 |
| __bench_read | Userspace | 10283.00 | 10332.37 | 10307.35 | 15.97 |
| __bench_uprobe | Kernel | 1224.20 | 1228.29 | 1226.15 | 1.48 |
| __bench_uprobe | Userspace | 361.77 | 371.53 | 366.02 | 3.35 |
| __bench_uprobe_uretprobe | Kernel | 1745.75 | 1803.48 | 1759.84 | 21.76 |
| __bench_uprobe_uretprobe | Userspace | 417.17 | 425.35 | 420.27 | 2.79 |
| __bench_uretprobe | Kernel | 1635.54 | 1661.15 | 1642.76 | 8.98 |
| __bench_uretprobe | Userspace | 360.69 | 367.51 | 363.74 | 2.32 |
| __bench_write | Kernel | 17604.15 | 17825.42 | 17693.42 | 72.86 |
| __bench_write | Userspace | 10356.40 | 10522.20 | 10436.07 | 66.64 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 192.85 | 367.54 | 211.65 | 51.99 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-09-09 22:00:24
- **Total duration:** 4406.68 seconds

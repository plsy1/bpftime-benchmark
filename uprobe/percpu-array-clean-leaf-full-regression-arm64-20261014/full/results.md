# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-14 22:14:59*

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
| __bench_uprobe | 1229.74 | 318.44 | 3.86x |
| __bench_uretprobe | 1647.70 | 317.75 | 5.19x |
| __bench_uprobe_uretprobe | 1754.43 | 333.55 | 5.26x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3516.04 | 3537.52 | 3523.35 | 6.53 |
| __bench_array_map_delete | Userspace | 15836.39 | 15927.08 | 15884.21 | 28.99 |
| __bench_array_map_lookup | Kernel | 3739.67 | 3787.17 | 3762.09 | 18.83 |
| __bench_array_map_lookup | Userspace | 15812.11 | 16022.08 | 15886.74 | 66.54 |
| __bench_array_map_update | Kernel | 13079.68 | 13122.16 | 13105.13 | 11.57 |
| __bench_array_map_update | Userspace | 23330.18 | 23535.06 | 23446.40 | 70.92 |
| __bench_hash_map_delete | Kernel | 107220.52 | 107327.64 | 107266.27 | 30.55 |
| __bench_hash_map_delete | Userspace | 40774.01 | 41363.67 | 41043.59 | 213.40 |
| __bench_hash_map_lookup | Kernel | 28299.26 | 28522.77 | 28388.08 | 65.31 |
| __bench_hash_map_lookup | Userspace | 56010.96 | 58249.41 | 57032.17 | 916.25 |
| __bench_hash_map_update | Kernel | 94074.41 | 94377.97 | 94248.26 | 76.76 |
| __bench_hash_map_update | Userspace | 63331.77 | 64013.27 | 63719.98 | 223.95 |
| __bench_per_cpu_array_map_delete | Kernel | 3645.48 | 3694.06 | 3682.93 | 12.99 |
| __bench_per_cpu_array_map_delete | Userspace | 15761.70 | 15931.02 | 15835.45 | 51.16 |
| __bench_per_cpu_array_map_lookup | Kernel | 4401.27 | 4411.71 | 4406.21 | 3.78 |
| __bench_per_cpu_array_map_lookup | Userspace | 11642.66 | 11765.09 | 11707.57 | 42.09 |
| __bench_per_cpu_array_map_update | Kernel | 16575.66 | 16630.40 | 16604.34 | 14.96 |
| __bench_per_cpu_array_map_update | Userspace | 26733.70 | 27126.81 | 26968.87 | 130.28 |
| __bench_per_cpu_hash_map_delete | Kernel | 151549.50 | 151777.05 | 151642.78 | 61.46 |
| __bench_per_cpu_hash_map_delete | Userspace | 108649.75 | 109192.78 | 108884.85 | 206.24 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29231.54 | 29378.54 | 29308.75 | 43.26 |
| __bench_per_cpu_hash_map_lookup | Userspace | 63533.55 | 63821.75 | 63687.84 | 102.29 |
| __bench_per_cpu_hash_map_update | Kernel | 67736.29 | 67850.83 | 67810.51 | 39.67 |
| __bench_per_cpu_hash_map_update | Userspace | 71209.84 | 71589.44 | 71385.85 | 121.80 |
| __bench_read | Kernel | 17594.42 | 17685.99 | 17641.76 | 25.35 |
| __bench_read | Userspace | 4686.10 | 4698.08 | 4690.96 | 3.27 |
| __bench_uprobe | Kernel | 1224.09 | 1241.56 | 1229.74 | 5.42 |
| __bench_uprobe | Userspace | 315.97 | 321.46 | 318.44 | 1.56 |
| __bench_uprobe_uretprobe | Kernel | 1746.19 | 1767.12 | 1754.43 | 6.19 |
| __bench_uprobe_uretprobe | Userspace | 331.88 | 335.23 | 333.55 | 1.13 |
| __bench_uretprobe | Kernel | 1636.42 | 1660.75 | 1647.70 | 8.39 |
| __bench_uretprobe | Userspace | 315.20 | 319.33 | 317.75 | 1.54 |
| __bench_write | Kernel | 17645.02 | 17696.51 | 17669.67 | 18.46 |
| __bench_write | Userspace | 4392.68 | 4398.92 | 4395.35 | 1.84 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 157.37 | 186.55 | 163.84 | 8.08 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-14 22:14:59
- **Total duration:** 1665.12 seconds

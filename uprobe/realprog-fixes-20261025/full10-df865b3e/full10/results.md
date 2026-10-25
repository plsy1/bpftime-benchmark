# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-25 19:39:40*

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
| __bench_array_map_delete | Userspace | 20825.28 | 21010.22 | 20876.27 | 67.36 |
| __bench_array_map_lookup | Userspace | 1168.66 | 1181.65 | 1173.17 | 3.29 |
| __bench_array_map_update | Userspace | 11463.79 | 11556.96 | 11514.24 | 29.52 |
| __bench_hash_map_delete | Userspace | 27141.99 | 29129.97 | 28118.61 | 677.41 |
| __bench_hash_map_lookup | Userspace | 22893.84 | 23669.59 | 23257.19 | 255.13 |
| __bench_hash_map_update | Userspace | 26591.90 | 34670.34 | 28440.67 | 2201.84 |
| __bench_per_cpu_array_map_delete | Userspace | 20816.05 | 21029.61 | 20893.39 | 72.76 |
| __bench_per_cpu_array_map_lookup | Userspace | 4106.22 | 4133.30 | 4119.00 | 9.87 |
| __bench_per_cpu_array_map_update | Userspace | 14340.47 | 14498.47 | 14416.93 | 56.84 |
| __bench_per_cpu_hash_map_delete | Userspace | 99263.76 | 99504.63 | 99361.80 | 71.05 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27605.39 | 27896.14 | 27731.75 | 90.83 |
| __bench_per_cpu_hash_map_update | Userspace | 44324.30 | 44469.53 | 44372.54 | 49.70 |
| __bench_read | Userspace | 4579.43 | 4589.28 | 4584.30 | 2.50 |
| __bench_uprobe | Userspace | 197.74 | 200.98 | 199.55 | 1.02 |
| __bench_uprobe_uretprobe | Userspace | 331.53 | 336.43 | 333.54 | 1.46 |
| __bench_uretprobe | Userspace | 273.43 | 280.02 | 276.33 | 1.82 |
| __bench_write | Userspace | 4272.13 | 4278.05 | 4274.68 | 1.69 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.98 | 183.29 | 158.77 | 8.55 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-25 19:39:40
- **Total duration:** 517.89 seconds

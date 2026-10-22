# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-22 16:45:09*

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
| __bench_array_map_delete | Userspace | 20094.51 | 20488.15 | 20209.60 | 124.79 |
| __bench_array_map_lookup | Userspace | 1167.92 | 1189.77 | 1173.59 | 5.57 |
| __bench_array_map_update | Userspace | 11480.47 | 11647.27 | 11540.92 | 47.25 |
| __bench_hash_map_delete | Userspace | 36635.68 | 37169.18 | 36863.81 | 212.53 |
| __bench_hash_map_lookup | Userspace | 23695.14 | 25036.67 | 24269.48 | 509.11 |
| __bench_hash_map_update | Userspace | 27308.79 | 37561.93 | 29740.82 | 3075.06 |
| __bench_per_cpu_array_map_delete | Userspace | 20173.76 | 20575.66 | 20292.71 | 131.13 |
| __bench_per_cpu_array_map_lookup | Userspace | 4100.54 | 4119.77 | 4112.43 | 6.43 |
| __bench_per_cpu_array_map_update | Userspace | 14472.75 | 15134.33 | 14569.15 | 189.65 |
| __bench_per_cpu_hash_map_delete | Userspace | 98664.90 | 100280.53 | 99053.25 | 513.74 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27621.96 | 28928.65 | 27861.17 | 359.48 |
| __bench_per_cpu_hash_map_update | Userspace | 44430.71 | 45640.80 | 44616.72 | 345.15 |
| __bench_read | Userspace | 4563.23 | 4568.53 | 4564.92 | 1.64 |
| __bench_uprobe | Userspace | 197.46 | 201.18 | 198.36 | 1.13 |
| __bench_uprobe_uretprobe | Userspace | 321.93 | 324.12 | 322.83 | 0.77 |
| __bench_uretprobe | Userspace | 268.13 | 270.68 | 269.41 | 0.82 |
| __bench_write | Userspace | 4271.14 | 4275.30 | 4272.35 | 1.34 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.95 | 173.85 | 158.32 | 5.71 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-22 16:45:09
- **Total duration:** 527.51 seconds

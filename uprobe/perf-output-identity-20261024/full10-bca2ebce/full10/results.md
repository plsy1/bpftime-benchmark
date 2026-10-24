# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-24 19:09:26*

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
| __bench_array_map_delete | Userspace | 20785.96 | 21935.92 | 20998.69 | 320.96 |
| __bench_array_map_lookup | Userspace | 1165.08 | 1173.44 | 1170.24 | 2.28 |
| __bench_array_map_update | Userspace | 11495.26 | 11697.03 | 11536.37 | 56.53 |
| __bench_hash_map_delete | Userspace | 27219.43 | 28540.35 | 27659.98 | 451.78 |
| __bench_hash_map_lookup | Userspace | 23839.62 | 24841.83 | 24621.76 | 388.36 |
| __bench_hash_map_update | Userspace | 27795.61 | 31600.43 | 30575.49 | 1398.43 |
| __bench_per_cpu_array_map_delete | Userspace | 20804.38 | 20971.87 | 20899.28 | 64.43 |
| __bench_per_cpu_array_map_lookup | Userspace | 4100.44 | 4129.19 | 4113.46 | 8.05 |
| __bench_per_cpu_array_map_update | Userspace | 14207.23 | 14600.60 | 14297.68 | 112.05 |
| __bench_per_cpu_hash_map_delete | Userspace | 98657.58 | 98795.07 | 98722.68 | 45.85 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27608.58 | 27770.65 | 27699.42 | 52.30 |
| __bench_per_cpu_hash_map_update | Userspace | 44232.80 | 44515.93 | 44405.91 | 72.52 |
| __bench_read | Userspace | 4589.68 | 4596.26 | 4593.84 | 2.16 |
| __bench_uprobe | Userspace | 197.10 | 202.59 | 199.12 | 1.53 |
| __bench_uprobe_uretprobe | Userspace | 323.27 | 328.31 | 324.43 | 1.44 |
| __bench_uretprobe | Userspace | 269.83 | 275.57 | 271.31 | 1.61 |
| __bench_write | Userspace | 4271.27 | 4276.00 | 4273.63 | 1.19 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 156.30 | 180.36 | 162.38 | 7.25 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-24 19:09:25
- **Total duration:** 525.44 seconds

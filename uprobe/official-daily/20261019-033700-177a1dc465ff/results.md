# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-19 04:01:50*

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
| __bench_uprobe | 1228.33 | 195.86 | 6.27x |
| __bench_uretprobe | 1642.88 | 268.15 | 6.13x |
| __bench_uprobe_uretprobe | 1749.92 | 318.81 | 5.49x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3673.71 | 3683.94 | 3677.05 | 3.93 |
| __bench_array_map_delete | Userspace | 20257.55 | 20607.14 | 20368.78 | 115.90 |
| __bench_array_map_lookup | Kernel | 3736.87 | 3745.44 | 3740.52 | 3.36 |
| __bench_array_map_lookup | Userspace | 19927.65 | 20922.70 | 20099.95 | 277.44 |
| __bench_array_map_update | Kernel | 13030.46 | 13059.84 | 13045.89 | 9.33 |
| __bench_array_map_update | Userspace | 25363.71 | 25413.99 | 25385.96 | 16.94 |
| __bench_hash_map_delete | Kernel | 52454.60 | 52523.47 | 52493.75 | 21.54 |
| __bench_hash_map_delete | Userspace | 150540.03 | 151057.18 | 150858.13 | 138.46 |
| __bench_hash_map_lookup | Kernel | 28134.92 | 28404.63 | 28263.26 | 73.80 |
| __bench_hash_map_lookup | Userspace | 51846.69 | 53868.61 | 52384.81 | 601.88 |
| __bench_hash_map_update | Kernel | 94342.30 | 94457.67 | 94399.64 | 36.68 |
| __bench_hash_map_update | Userspace | 61711.76 | 62626.04 | 62024.39 | 292.37 |
| __bench_per_cpu_array_map_delete | Kernel | 3516.97 | 3519.65 | 3518.06 | 0.81 |
| __bench_per_cpu_array_map_delete | Userspace | 20163.12 | 20551.03 | 20327.74 | 125.03 |
| __bench_per_cpu_array_map_lookup | Kernel | 4204.11 | 4228.81 | 4220.99 | 6.83 |
| __bench_per_cpu_array_map_lookup | Userspace | 28780.25 | 29807.34 | 28998.96 | 290.09 |
| __bench_per_cpu_array_map_update | Kernel | 16616.62 | 16674.20 | 16640.69 | 18.64 |
| __bench_per_cpu_array_map_update | Userspace | 74980.77 | 75115.51 | 75029.82 | 37.66 |
| __bench_per_cpu_hash_map_delete | Kernel | 52402.34 | 52437.24 | 52416.31 | 9.14 |
| __bench_per_cpu_hash_map_delete | Userspace | 258078.18 | 258440.71 | 258201.07 | 107.04 |
| __bench_per_cpu_hash_map_lookup | Kernel | 28576.92 | 28688.73 | 28628.50 | 33.45 |
| __bench_per_cpu_hash_map_lookup | Userspace | 126994.75 | 127463.97 | 127236.71 | 137.45 |
| __bench_per_cpu_hash_map_update | Kernel | 67485.76 | 67602.91 | 67518.21 | 39.08 |
| __bench_per_cpu_hash_map_update | Userspace | 214608.69 | 215973.40 | 215389.58 | 520.57 |
| __bench_read | Kernel | 17969.44 | 18913.65 | 18155.79 | 260.68 |
| __bench_read | Userspace | 4568.93 | 4598.03 | 4575.76 | 8.64 |
| __bench_uprobe | Kernel | 1223.94 | 1236.66 | 1228.33 | 4.87 |
| __bench_uprobe | Userspace | 194.75 | 196.80 | 195.86 | 0.72 |
| __bench_uprobe_uretprobe | Kernel | 1741.44 | 1770.27 | 1749.92 | 8.46 |
| __bench_uprobe_uretprobe | Userspace | 317.65 | 320.09 | 318.81 | 0.66 |
| __bench_uretprobe | Kernel | 1636.84 | 1662.73 | 1642.88 | 7.91 |
| __bench_uretprobe | Userspace | 267.31 | 269.02 | 268.15 | 0.56 |
| __bench_write | Kernel | 17630.22 | 17809.85 | 17711.83 | 55.41 |
| __bench_write | Userspace | 4269.24 | 4273.27 | 4271.79 | 1.23 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.69 | 174.85 | 157.34 | 6.28 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-19 04:01:50
- **Total duration:** 1490.38 seconds

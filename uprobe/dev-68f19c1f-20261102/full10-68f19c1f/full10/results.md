# BPFtime Uprobe Benchmark Results

*Generated on 2026-11-02 05:33:47*

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
| __bench_uprobe | 1243.63 | 199.82 | 6.22x |
| __bench_uretprobe | 1672.66 | 273.36 | 6.12x |
| __bench_uprobe_uretprobe | 1781.30 | 328.19 | 5.43x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3745.08 | 3755.09 | 3748.54 | 3.58 |
| __bench_array_map_delete | Userspace | 20904.66 | 21050.05 | 20994.80 | 41.36 |
| __bench_array_map_lookup | Kernel | 3789.92 | 3812.06 | 3798.19 | 7.35 |
| __bench_array_map_lookup | Userspace | 1168.44 | 1178.50 | 1175.89 | 2.75 |
| __bench_array_map_update | Kernel | 13102.92 | 13150.61 | 13127.76 | 11.74 |
| __bench_array_map_update | Userspace | 11531.87 | 11786.24 | 11569.71 | 72.92 |
| __bench_hash_map_delete | Kernel | 108291.61 | 109006.50 | 108499.01 | 235.88 |
| __bench_hash_map_delete | Userspace | 37455.11 | 37644.86 | 37525.86 | 54.98 |
| __bench_hash_map_lookup | Kernel | 28883.06 | 29178.89 | 28975.86 | 84.96 |
| __bench_hash_map_lookup | Userspace | 22305.11 | 22350.70 | 22329.23 | 14.94 |
| __bench_hash_map_update | Kernel | 95655.40 | 96343.53 | 95855.64 | 241.76 |
| __bench_hash_map_update | Userspace | 29481.83 | 29550.61 | 29511.66 | 26.24 |
| __bench_per_cpu_array_map_delete | Kernel | 3744.08 | 3771.78 | 3751.31 | 8.02 |
| __bench_per_cpu_array_map_delete | Userspace | 20958.34 | 21138.03 | 21066.96 | 57.11 |
| __bench_per_cpu_array_map_lookup | Kernel | 4520.84 | 4548.79 | 4525.67 | 8.75 |
| __bench_per_cpu_array_map_lookup | Userspace | 4100.49 | 4123.12 | 4113.82 | 6.68 |
| __bench_per_cpu_array_map_update | Kernel | 16625.93 | 16656.19 | 16640.96 | 11.58 |
| __bench_per_cpu_array_map_update | Userspace | 14783.15 | 15088.09 | 14882.45 | 108.12 |
| __bench_per_cpu_hash_map_delete | Kernel | 150426.96 | 150528.10 | 150473.72 | 31.37 |
| __bench_per_cpu_hash_map_delete | Userspace | 104025.75 | 105099.65 | 104641.26 | 384.98 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29244.85 | 29525.08 | 29406.04 | 82.60 |
| __bench_per_cpu_hash_map_lookup | Userspace | 32776.30 | 33922.79 | 33345.59 | 419.01 |
| __bench_per_cpu_hash_map_update | Kernel | 67856.16 | 67912.45 | 67890.99 | 18.17 |
| __bench_per_cpu_hash_map_update | Userspace | 53881.18 | 55251.24 | 54334.44 | 403.39 |
| __bench_read | Kernel | 17579.39 | 17848.87 | 17648.80 | 73.79 |
| __bench_read | Userspace | 4563.81 | 4569.61 | 4566.11 | 2.00 |
| __bench_uprobe | Kernel | 1237.88 | 1258.73 | 1243.63 | 6.78 |
| __bench_uprobe | Userspace | 197.86 | 203.62 | 199.82 | 1.88 |
| __bench_uprobe_uretprobe | Kernel | 1770.52 | 1797.84 | 1781.30 | 9.36 |
| __bench_uprobe_uretprobe | Userspace | 325.79 | 335.21 | 328.19 | 2.80 |
| __bench_uretprobe | Kernel | 1655.85 | 1699.56 | 1672.66 | 15.78 |
| __bench_uretprobe | Userspace | 271.54 | 278.74 | 273.36 | 1.92 |
| __bench_write | Kernel | 17741.82 | 18015.80 | 17783.37 | 79.26 |
| __bench_write | Userspace | 4272.40 | 4277.93 | 4274.90 | 1.61 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 154.83 | 177.98 | 158.63 | 6.65 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-11-02 05:33:47
- **Total duration:** 1453.84 seconds

# BPFtime Uprobe Benchmark Results

*Generated on 2026-11-04 01:17:54*

## Environment

- **OS:** Linux 6.12.113+deb13-amd64
- **CPU:** 12th Gen Intel(R) Core(TM) i5-1240P (12 cores, 16 threads)
- **Memory:** 62.39 GB
- **Python:** 3.13.5

## Summary

This benchmark compares three different eBPF execution environments:
- **Kernel Uprobe**: Traditional kernel-based eBPF uprobes
- **Userspace Uprobe**: BPFtime's userspace implementation of uprobes
- **Embedded VM**: BPFtime's embedded eBPF VM

*Times shown in nanoseconds (ns) - lower is better*

### Core Uprobe Performance Summary

| Operation | Kernel Uprobe | Userspace Uprobe | Speedup |
|-----------|---------------|------------------|---------|
| __bench_uprobe | 1110.09 | 123.35 | 9.00x |
| __bench_uretprobe | 1294.81 | 184.81 | 7.01x |
| __bench_uprobe_uretprobe | 1341.20 | 216.37 | 6.20x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 1733.52 | 1747.55 | 1741.77 | 4.18 |
| __bench_array_map_delete | Userspace | 5402.42 | 5443.84 | 5411.96 | 11.09 |
| __bench_array_map_lookup | Kernel | 1808.64 | 1832.56 | 1819.38 | 7.11 |
| __bench_array_map_lookup | Userspace | 445.05 | 446.30 | 445.72 | 0.35 |
| __bench_array_map_update | Kernel | 7403.22 | 7740.96 | 7481.53 | 95.53 |
| __bench_array_map_update | Userspace | 3342.93 | 3515.31 | 3374.30 | 53.83 |
| __bench_hash_map_delete | Kernel | 34212.23 | 34362.20 | 34263.79 | 39.21 |
| __bench_hash_map_delete | Userspace | 14376.31 | 14869.40 | 14605.81 | 144.65 |
| __bench_hash_map_lookup | Kernel | 10712.72 | 10820.02 | 10758.91 | 31.75 |
| __bench_hash_map_lookup | Userspace | 4845.98 | 5249.62 | 4944.46 | 149.84 |
| __bench_hash_map_update | Kernel | 34151.85 | 35094.32 | 34646.96 | 398.04 |
| __bench_hash_map_update | Userspace | 11595.31 | 12139.26 | 11776.82 | 160.91 |
| __bench_per_cpu_array_map_delete | Kernel | 1738.30 | 1951.79 | 1769.17 | 61.16 |
| __bench_per_cpu_array_map_delete | Userspace | 5403.42 | 5444.22 | 5413.89 | 11.70 |
| __bench_per_cpu_array_map_lookup | Kernel | 1808.66 | 1821.38 | 1816.37 | 3.67 |
| __bench_per_cpu_array_map_lookup | Userspace | 1278.15 | 1280.67 | 1278.83 | 0.76 |
| __bench_per_cpu_array_map_update | Kernel | 7426.83 | 7485.37 | 7454.31 | 23.07 |
| __bench_per_cpu_array_map_update | Userspace | 4787.31 | 4913.50 | 4839.12 | 49.13 |
| __bench_per_cpu_hash_map_delete | Kernel | 112461.67 | 112896.91 | 112626.21 | 110.16 |
| __bench_per_cpu_hash_map_delete | Userspace | 25440.77 | 25976.84 | 25619.61 | 140.13 |
| __bench_per_cpu_hash_map_lookup | Kernel | 10377.95 | 10787.10 | 10543.08 | 135.50 |
| __bench_per_cpu_hash_map_lookup | Userspace | 6895.34 | 7068.77 | 6971.26 | 60.20 |
| __bench_per_cpu_hash_map_update | Kernel | 25435.88 | 26360.69 | 25866.77 | 376.43 |
| __bench_per_cpu_hash_map_update | Userspace | 11784.56 | 12023.89 | 11870.38 | 81.34 |
| __bench_read | Kernel | 17958.82 | 19732.53 | 19096.36 | 584.60 |
| __bench_read | Userspace | 1503.84 | 1513.51 | 1506.37 | 2.88 |
| __bench_uprobe | Kernel | 1102.83 | 1115.66 | 1110.09 | 3.62 |
| __bench_uprobe | Userspace | 122.39 | 124.38 | 123.35 | 0.49 |
| __bench_uprobe_uretprobe | Kernel | 1334.04 | 1345.75 | 1341.20 | 3.92 |
| __bench_uprobe_uretprobe | Userspace | 215.20 | 217.14 | 216.37 | 0.62 |
| __bench_uretprobe | Kernel | 1288.61 | 1299.16 | 1294.81 | 3.26 |
| __bench_uretprobe | Userspace | 183.31 | 186.11 | 184.81 | 0.79 |
| __bench_write | Kernel | 19093.80 | 19601.89 | 19207.24 | 149.78 |
| __bench_write | Userspace | 1277.91 | 1282.92 | 1279.72 | 1.72 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 62.52 | 63.85 | 63.17 | 0.42 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-11-04 01:17:54
- **Total duration:** 606.36 seconds

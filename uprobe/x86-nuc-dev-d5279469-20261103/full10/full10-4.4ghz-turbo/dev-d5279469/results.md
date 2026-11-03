# BPFtime Uprobe Benchmark Results

*Generated on 2026-11-03 22:58:46*

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
| __bench_uprobe | 1110.27 | 122.92 | 9.03x |
| __bench_uretprobe | 1297.82 | 184.43 | 7.04x |
| __bench_uprobe_uretprobe | 1355.60 | 216.16 | 6.27x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 1740.31 | 1749.87 | 1743.93 | 2.62 |
| __bench_array_map_delete | Userspace | 5404.00 | 5444.19 | 5415.12 | 12.68 |
| __bench_array_map_lookup | Kernel | 1815.26 | 1825.94 | 1818.84 | 3.30 |
| __bench_array_map_lookup | Userspace | 445.05 | 446.80 | 445.79 | 0.51 |
| __bench_array_map_update | Kernel | 7415.53 | 7621.08 | 7443.40 | 59.53 |
| __bench_array_map_update | Userspace | 3342.92 | 3532.47 | 3386.93 | 70.52 |
| __bench_hash_map_delete | Kernel | 34151.64 | 34352.77 | 34249.86 | 56.40 |
| __bench_hash_map_delete | Userspace | 14346.00 | 15199.45 | 14608.64 | 242.81 |
| __bench_hash_map_lookup | Kernel | 10082.04 | 10209.16 | 10136.44 | 45.17 |
| __bench_hash_map_lookup | Userspace | 4654.19 | 5376.75 | 4766.33 | 204.61 |
| __bench_hash_map_update | Kernel | 34576.22 | 34779.03 | 34654.30 | 66.18 |
| __bench_hash_map_update | Userspace | 11859.39 | 12178.13 | 12007.31 | 104.75 |
| __bench_per_cpu_array_map_delete | Kernel | 1743.64 | 1760.51 | 1750.46 | 5.52 |
| __bench_per_cpu_array_map_delete | Userspace | 5405.66 | 5419.04 | 5410.25 | 4.03 |
| __bench_per_cpu_array_map_lookup | Kernel | 1817.70 | 1829.75 | 1822.82 | 3.78 |
| __bench_per_cpu_array_map_lookup | Userspace | 1277.47 | 1281.66 | 1279.09 | 1.10 |
| __bench_per_cpu_array_map_update | Kernel | 7492.20 | 7738.51 | 7538.67 | 78.62 |
| __bench_per_cpu_array_map_update | Userspace | 4726.65 | 4959.94 | 4849.24 | 76.74 |
| __bench_per_cpu_hash_map_delete | Kernel | 111324.58 | 111928.84 | 111628.81 | 174.10 |
| __bench_per_cpu_hash_map_delete | Userspace | 25480.16 | 25929.41 | 25653.33 | 159.38 |
| __bench_per_cpu_hash_map_lookup | Kernel | 12497.95 | 12920.65 | 12643.12 | 112.77 |
| __bench_per_cpu_hash_map_lookup | Userspace | 6972.58 | 7116.71 | 7015.62 | 52.95 |
| __bench_per_cpu_hash_map_update | Kernel | 25796.00 | 26140.67 | 25895.36 | 117.71 |
| __bench_per_cpu_hash_map_update | Userspace | 11931.93 | 12136.70 | 11993.27 | 52.99 |
| __bench_read | Kernel | 17932.29 | 19565.80 | 19097.20 | 584.08 |
| __bench_read | Userspace | 1505.07 | 1512.80 | 1506.66 | 2.14 |
| __bench_uprobe | Kernel | 1107.43 | 1116.26 | 1110.27 | 2.73 |
| __bench_uprobe | Userspace | 121.65 | 124.31 | 122.92 | 0.69 |
| __bench_uprobe_uretprobe | Kernel | 1338.54 | 1449.96 | 1355.60 | 31.86 |
| __bench_uprobe_uretprobe | Userspace | 214.32 | 217.51 | 216.16 | 0.89 |
| __bench_uretprobe | Kernel | 1294.80 | 1302.07 | 1297.82 | 2.52 |
| __bench_uretprobe | Userspace | 182.55 | 185.50 | 184.43 | 0.79 |
| __bench_write | Kernel | 19096.74 | 19333.85 | 19134.64 | 67.57 |
| __bench_write | Userspace | 1278.08 | 1285.46 | 1279.90 | 2.05 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 62.52 | 65.94 | 63.66 | 0.84 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-11-03 22:58:46
- **Total duration:** 607.31 seconds

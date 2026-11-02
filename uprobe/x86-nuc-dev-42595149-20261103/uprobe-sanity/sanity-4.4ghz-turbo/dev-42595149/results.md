# BPFtime Uprobe Benchmark Results

*Generated on 2026-11-03 01:36:47*

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
| __bench_uprobe | 1103.91 | 125.98 | 8.76x |
| __bench_uretprobe | 1288.94 | 186.34 | 6.92x |
| __bench_uprobe_uretprobe | 1335.82 | 217.76 | 6.13x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 1744.24 | 1758.40 | 1751.32 | 7.08 |
| __bench_array_map_delete | Userspace | 5902.94 | 5907.91 | 5905.42 | 2.49 |
| __bench_array_map_lookup | Kernel | 1806.39 | 1816.85 | 1811.62 | 5.23 |
| __bench_array_map_lookup | Userspace | 445.69 | 446.52 | 446.11 | 0.41 |
| __bench_array_map_update | Kernel | 7415.44 | 7424.27 | 7419.85 | 4.41 |
| __bench_array_map_update | Userspace | 3633.37 | 3777.93 | 3705.65 | 72.28 |
| __bench_hash_map_delete | Kernel | 34133.95 | 34140.52 | 34137.23 | 3.28 |
| __bench_hash_map_delete | Userspace | 15558.23 | 15613.92 | 15586.08 | 27.84 |
| __bench_hash_map_lookup | Kernel | 10245.89 | 10332.17 | 10289.03 | 43.14 |
| __bench_hash_map_lookup | Userspace | 5272.36 | 5294.08 | 5283.22 | 10.86 |
| __bench_hash_map_update | Kernel | 34421.28 | 34469.75 | 34445.51 | 24.23 |
| __bench_hash_map_update | Userspace | 9997.48 | 10042.78 | 10020.13 | 22.65 |
| __bench_per_cpu_array_map_delete | Kernel | 1732.12 | 1747.26 | 1739.69 | 7.57 |
| __bench_per_cpu_array_map_delete | Userspace | 5942.47 | 5947.49 | 5944.98 | 2.51 |
| __bench_per_cpu_array_map_lookup | Kernel | 1809.17 | 1819.20 | 1814.18 | 5.01 |
| __bench_per_cpu_array_map_lookup | Userspace | 1277.73 | 1278.16 | 1277.94 | 0.22 |
| __bench_per_cpu_array_map_update | Kernel | 7465.88 | 7553.07 | 7509.47 | 43.60 |
| __bench_per_cpu_array_map_update | Userspace | 4934.41 | 4945.11 | 4939.76 | 5.35 |
| __bench_per_cpu_hash_map_delete | Kernel | 111954.51 | 112057.43 | 112005.97 | 51.46 |
| __bench_per_cpu_hash_map_delete | Userspace | 25140.50 | 25190.74 | 25165.62 | 25.12 |
| __bench_per_cpu_hash_map_lookup | Kernel | 11304.53 | 11448.64 | 11376.59 | 72.05 |
| __bench_per_cpu_hash_map_lookup | Userspace | 7184.87 | 7211.71 | 7198.29 | 13.42 |
| __bench_per_cpu_hash_map_update | Kernel | 25742.09 | 25782.88 | 25762.49 | 20.40 |
| __bench_per_cpu_hash_map_update | Userspace | 11668.13 | 11686.68 | 11677.40 | 9.28 |
| __bench_read | Kernel | 19315.25 | 19354.94 | 19335.10 | 19.85 |
| __bench_read | Userspace | 1507.00 | 1507.68 | 1507.34 | 0.34 |
| __bench_uprobe | Kernel | 1102.59 | 1105.24 | 1103.91 | 1.32 |
| __bench_uprobe | Userspace | 125.92 | 126.05 | 125.98 | 0.06 |
| __bench_uprobe_uretprobe | Kernel | 1333.23 | 1338.41 | 1335.82 | 2.59 |
| __bench_uprobe_uretprobe | Userspace | 216.98 | 218.55 | 217.76 | 0.78 |
| __bench_uretprobe | Kernel | 1287.67 | 1290.21 | 1288.94 | 1.27 |
| __bench_uretprobe | Userspace | 186.04 | 186.64 | 186.34 | 0.30 |
| __bench_write | Kernel | 19052.69 | 19138.65 | 19095.67 | 42.98 |
| __bench_write | Userspace | 1279.27 | 1281.96 | 1280.61 | 1.34 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 62.17 | 63.49 | 62.83 | 0.66 |

## Benchmark Metadata

- **Number of runs:** 2
- **Timestamp:** 2026-11-03 01:36:47
- **Total duration:** 130.50 seconds

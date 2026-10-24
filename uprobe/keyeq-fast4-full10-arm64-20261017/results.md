# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-17 18:34:20*

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
| __bench_uprobe | 1228.85 | 197.02 | 6.24x |
| __bench_uretprobe | 1649.49 | 267.69 | 6.16x |
| __bench_uprobe_uretprobe | 1761.97 | 319.40 | 5.52x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3684.00 | 3701.72 | 3688.29 | 5.42 |
| __bench_array_map_delete | Userspace | 19997.67 | 20201.30 | 20147.06 | 68.04 |
| __bench_array_map_lookup | Kernel | 3788.98 | 3802.79 | 3792.77 | 4.10 |
| __bench_array_map_lookup | Userspace | 7918.27 | 7955.12 | 7927.84 | 12.09 |
| __bench_array_map_update | Kernel | 12999.72 | 13050.19 | 13031.38 | 14.14 |
| __bench_array_map_update | Userspace | 11517.01 | 11585.83 | 11540.18 | 21.00 |
| __bench_hash_map_delete | Kernel | 105940.09 | 106021.36 | 105971.59 | 31.63 |
| __bench_hash_map_delete | Userspace | 36627.47 | 37011.28 | 36802.20 | 124.94 |
| __bench_hash_map_lookup | Kernel | 27948.12 | 28214.68 | 28063.72 | 79.09 |
| __bench_hash_map_lookup | Userspace | 29509.31 | 30023.59 | 29663.38 | 141.11 |
| __bench_hash_map_update | Kernel | 93086.47 | 93294.32 | 93131.29 | 57.18 |
| __bench_hash_map_update | Userspace | 51531.80 | 52780.20 | 52249.77 | 343.02 |
| __bench_per_cpu_array_map_delete | Kernel | 3681.71 | 3693.02 | 3684.52 | 3.44 |
| __bench_per_cpu_array_map_delete | Userspace | 20077.28 | 20236.31 | 20138.54 | 45.10 |
| __bench_per_cpu_array_map_lookup | Kernel | 4422.88 | 4440.83 | 4426.47 | 5.06 |
| __bench_per_cpu_array_map_lookup | Userspace | 11594.80 | 11724.14 | 11643.84 | 48.38 |
| __bench_per_cpu_array_map_update | Kernel | 16541.29 | 16608.48 | 16571.17 | 21.57 |
| __bench_per_cpu_array_map_update | Userspace | 27720.34 | 27941.01 | 27806.43 | 72.29 |
| __bench_per_cpu_hash_map_delete | Kernel | 150753.33 | 150829.19 | 150778.59 | 20.80 |
| __bench_per_cpu_hash_map_delete | Userspace | 98742.03 | 99586.75 | 99223.53 | 341.11 |
| __bench_per_cpu_hash_map_lookup | Kernel | 28734.07 | 28893.87 | 28819.94 | 44.92 |
| __bench_per_cpu_hash_map_lookup | Userspace | 41452.70 | 41749.12 | 41619.84 | 91.41 |
| __bench_per_cpu_hash_map_update | Kernel | 67544.18 | 67580.33 | 67562.71 | 8.97 |
| __bench_per_cpu_hash_map_update | Userspace | 61347.77 | 61954.11 | 61637.72 | 175.56 |
| __bench_read | Kernel | 18020.44 | 18186.69 | 18097.85 | 53.64 |
| __bench_read | Userspace | 4589.00 | 4600.45 | 4593.62 | 3.64 |
| __bench_uprobe | Kernel | 1224.32 | 1238.18 | 1228.85 | 4.66 |
| __bench_uprobe | Userspace | 195.28 | 199.17 | 197.02 | 1.26 |
| __bench_uprobe_uretprobe | Kernel | 1750.13 | 1778.97 | 1761.97 | 9.61 |
| __bench_uprobe_uretprobe | Userspace | 318.78 | 321.88 | 319.40 | 0.88 |
| __bench_uretprobe | Kernel | 1636.91 | 1668.82 | 1649.49 | 9.80 |
| __bench_uretprobe | Userspace | 266.77 | 269.76 | 267.69 | 0.94 |
| __bench_write | Kernel | 17684.46 | 17774.88 | 17711.62 | 23.45 |
| __bench_write | Userspace | 4271.00 | 4845.29 | 4474.02 | 237.80 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 155.80 | 312.60 | 172.52 | 46.70 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-17 18:34:20
- **Total duration:** 1595.31 seconds

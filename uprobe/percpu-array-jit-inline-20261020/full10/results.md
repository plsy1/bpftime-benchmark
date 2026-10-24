# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-20 17:14:15*

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
| __bench_uprobe | 1227.68 | 198.96 | 6.17x |
| __bench_uretprobe | 1661.67 | 268.55 | 6.19x |
| __bench_uprobe_uretprobe | 1773.07 | 323.98 | 5.47x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3649.69 | 3701.24 | 3686.53 | 13.32 |
| __bench_array_map_delete | Userspace | 20610.47 | 21830.65 | 20826.52 | 349.04 |
| __bench_array_map_lookup | Kernel | 3805.84 | 3817.91 | 3813.34 | 3.96 |
| __bench_array_map_lookup | Userspace | 1173.22 | 1176.70 | 1175.07 | 1.00 |
| __bench_array_map_update | Kernel | 13077.16 | 13134.31 | 13100.96 | 16.04 |
| __bench_array_map_update | Userspace | 11488.97 | 12020.56 | 11585.46 | 150.10 |
| __bench_hash_map_delete | Kernel | 107657.64 | 107792.24 | 107702.80 | 37.35 |
| __bench_hash_map_delete | Userspace | 36881.45 | 37030.96 | 36938.68 | 44.94 |
| __bench_hash_map_lookup | Kernel | 29196.02 | 29447.89 | 29329.44 | 75.47 |
| __bench_hash_map_lookup | Userspace | 26830.88 | 27330.81 | 27074.05 | 113.69 |
| __bench_hash_map_update | Kernel | 94203.55 | 94359.21 | 94301.81 | 45.36 |
| __bench_hash_map_update | Userspace | 33402.60 | 35984.64 | 35632.69 | 745.05 |
| __bench_per_cpu_array_map_delete | Kernel | 3675.32 | 3684.60 | 3678.42 | 3.32 |
| __bench_per_cpu_array_map_delete | Userspace | 20655.66 | 20825.69 | 20733.73 | 63.51 |
| __bench_per_cpu_array_map_lookup | Kernel | 4427.05 | 4431.63 | 4429.51 | 1.30 |
| __bench_per_cpu_array_map_lookup | Userspace | 5806.74 | 5817.25 | 5811.72 | 3.35 |
| __bench_per_cpu_array_map_update | Kernel | 16556.73 | 16584.74 | 16567.55 | 8.98 |
| __bench_per_cpu_array_map_update | Userspace | 14449.70 | 14589.71 | 14518.22 | 46.29 |
| __bench_per_cpu_hash_map_delete | Kernel | 151650.27 | 151691.95 | 151667.84 | 13.71 |
| __bench_per_cpu_hash_map_delete | Userspace | 99639.87 | 100369.29 | 100175.57 | 204.26 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29252.90 | 29424.27 | 29326.29 | 55.04 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27591.75 | 27767.48 | 27671.15 | 60.13 |
| __bench_per_cpu_hash_map_update | Kernel | 67628.33 | 67691.59 | 67662.97 | 17.84 |
| __bench_per_cpu_hash_map_update | Userspace | 44432.70 | 44633.86 | 44523.31 | 48.29 |
| __bench_read | Kernel | 18071.30 | 18117.90 | 18095.70 | 15.70 |
| __bench_read | Userspace | 4569.15 | 4594.26 | 4579.05 | 9.38 |
| __bench_uprobe | Kernel | 1224.34 | 1235.61 | 1227.68 | 3.68 |
| __bench_uprobe | Userspace | 197.59 | 201.14 | 198.96 | 1.08 |
| __bench_uprobe_uretprobe | Kernel | 1768.14 | 1778.31 | 1773.07 | 2.92 |
| __bench_uprobe_uretprobe | Userspace | 323.09 | 325.41 | 323.98 | 0.63 |
| __bench_uretprobe | Kernel | 1656.06 | 1677.15 | 1661.67 | 7.57 |
| __bench_uretprobe | Userspace | 267.84 | 269.47 | 268.55 | 0.56 |
| __bench_write | Kernel | 17536.53 | 17710.30 | 17665.86 | 59.24 |
| __bench_write | Userspace | 4272.26 | 4275.81 | 4273.79 | 1.16 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 153.56 | 173.23 | 156.63 | 5.76 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-20 17:14:15
- **Total duration:** 1494.85 seconds

# BPFtime Uprobe Benchmark Results

*Generated on 2026-11-02 20:38:51*

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
| __bench_uprobe | 1239.61 | 201.45 | 6.15x |
| __bench_uretprobe | 1663.28 | 271.25 | 6.13x |
| __bench_uprobe_uretprobe | 1779.97 | 328.49 | 5.42x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3741.69 | 3758.28 | 3746.76 | 4.49 |
| __bench_array_map_delete | Userspace | 20873.39 | 22436.39 | 21121.06 | 444.41 |
| __bench_array_map_lookup | Kernel | 3814.64 | 3827.88 | 3822.34 | 4.37 |
| __bench_array_map_lookup | Userspace | 1174.28 | 1184.66 | 1178.25 | 2.68 |
| __bench_array_map_update | Kernel | 13133.25 | 13160.27 | 13143.64 | 9.63 |
| __bench_array_map_update | Userspace | 11508.49 | 12636.97 | 11652.37 | 329.05 |
| __bench_hash_map_delete | Kernel | 108572.88 | 108758.65 | 108667.07 | 52.44 |
| __bench_hash_map_delete | Userspace | 33858.93 | 34225.97 | 33940.14 | 102.89 |
| __bench_hash_map_lookup | Kernel | 29579.82 | 29748.18 | 29672.02 | 50.55 |
| __bench_hash_map_lookup | Userspace | 22228.52 | 22540.71 | 22286.75 | 86.92 |
| __bench_hash_map_update | Kernel | 95025.04 | 95184.22 | 95080.32 | 42.61 |
| __bench_hash_map_update | Userspace | 29057.66 | 29412.53 | 29130.75 | 96.97 |
| __bench_per_cpu_array_map_delete | Kernel | 3743.66 | 3756.26 | 3748.60 | 3.97 |
| __bench_per_cpu_array_map_delete | Userspace | 20971.54 | 22397.46 | 21332.51 | 373.75 |
| __bench_per_cpu_array_map_lookup | Kernel | 4520.25 | 4535.43 | 4524.55 | 4.64 |
| __bench_per_cpu_array_map_lookup | Userspace | 4100.37 | 4133.90 | 4114.76 | 10.55 |
| __bench_per_cpu_array_map_update | Kernel | 16584.52 | 16620.29 | 16598.28 | 10.24 |
| __bench_per_cpu_array_map_update | Userspace | 14779.08 | 15531.22 | 14917.77 | 207.09 |
| __bench_per_cpu_hash_map_delete | Kernel | 151024.16 | 151199.22 | 151098.10 | 59.40 |
| __bench_per_cpu_hash_map_delete | Userspace | 58092.59 | 58849.64 | 58339.49 | 225.39 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29663.21 | 29939.60 | 29795.65 | 84.11 |
| __bench_per_cpu_hash_map_lookup | Userspace | 25042.82 | 25580.84 | 25277.95 | 142.36 |
| __bench_per_cpu_hash_map_update | Kernel | 68220.15 | 68347.57 | 68269.37 | 39.55 |
| __bench_per_cpu_hash_map_update | Userspace | 36052.27 | 36456.60 | 36307.53 | 126.95 |
| __bench_read | Kernel | 17567.13 | 17645.49 | 17603.25 | 24.10 |
| __bench_read | Userspace | 4851.36 | 4856.57 | 4854.49 | 1.61 |
| __bench_uprobe | Kernel | 1234.18 | 1256.27 | 1239.61 | 6.29 |
| __bench_uprobe | Userspace | 199.58 | 204.12 | 201.45 | 1.26 |
| __bench_uprobe_uretprobe | Kernel | 1769.11 | 1808.91 | 1779.97 | 13.82 |
| __bench_uprobe_uretprobe | Userspace | 326.89 | 333.78 | 328.49 | 1.92 |
| __bench_uretprobe | Kernel | 1657.73 | 1675.78 | 1663.28 | 5.57 |
| __bench_uretprobe | Userspace | 269.24 | 275.07 | 271.25 | 1.55 |
| __bench_write | Kernel | 17750.59 | 18375.10 | 17825.60 | 183.63 |
| __bench_write | Userspace | 4275.18 | 4849.54 | 4409.81 | 220.33 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.53 | 173.57 | 157.60 | 6.02 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-11-02 20:38:51
- **Total duration:** 1296.28 seconds

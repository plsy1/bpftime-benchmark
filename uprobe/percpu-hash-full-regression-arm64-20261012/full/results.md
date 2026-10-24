# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-12 23:52:52*

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
| __bench_uprobe | 1239.50 | 317.77 | 3.90x |
| __bench_uretprobe | 1651.89 | 318.05 | 5.19x |
| __bench_uprobe_uretprobe | 1764.21 | 333.69 | 5.29x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3662.65 | 3683.33 | 3677.93 | 5.97 |
| __bench_array_map_delete | Userspace | 16305.82 | 16421.79 | 16353.59 | 44.96 |
| __bench_array_map_lookup | Kernel | 3803.56 | 3813.56 | 3807.08 | 3.57 |
| __bench_array_map_lookup | Userspace | 15840.96 | 16088.81 | 15946.18 | 65.96 |
| __bench_array_map_update | Kernel | 13082.55 | 13152.34 | 13113.01 | 21.54 |
| __bench_array_map_update | Userspace | 23538.27 | 24113.44 | 23637.22 | 161.84 |
| __bench_hash_map_delete | Kernel | 108071.28 | 108183.51 | 108141.93 | 33.17 |
| __bench_hash_map_delete | Userspace | 41871.12 | 43736.07 | 42704.39 | 648.21 |
| __bench_hash_map_lookup | Kernel | 28584.95 | 28878.09 | 28678.46 | 73.61 |
| __bench_hash_map_lookup | Userspace | 57736.94 | 59861.42 | 58729.82 | 912.73 |
| __bench_hash_map_update | Kernel | 92843.30 | 92955.48 | 92891.78 | 34.84 |
| __bench_hash_map_update | Userspace | 62217.84 | 63738.47 | 63019.05 | 663.59 |
| __bench_per_cpu_array_map_delete | Kernel | 3682.38 | 3724.02 | 3691.71 | 13.37 |
| __bench_per_cpu_array_map_delete | Userspace | 16302.39 | 16498.61 | 16399.48 | 69.39 |
| __bench_per_cpu_array_map_lookup | Kernel | 4399.89 | 4407.29 | 4403.14 | 2.46 |
| __bench_per_cpu_array_map_lookup | Userspace | 21362.14 | 21574.10 | 21468.53 | 68.22 |
| __bench_per_cpu_array_map_update | Kernel | 16589.64 | 16660.54 | 16621.29 | 20.16 |
| __bench_per_cpu_array_map_update | Userspace | 26981.50 | 27195.50 | 27052.91 | 62.35 |
| __bench_per_cpu_hash_map_delete | Kernel | 150356.94 | 150480.41 | 150429.57 | 36.94 |
| __bench_per_cpu_hash_map_delete | Userspace | 982982.05 | 988440.20 | 984889.22 | 1630.27 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29329.28 | 29518.28 | 29420.59 | 74.70 |
| __bench_per_cpu_hash_map_lookup | Userspace | 132085.48 | 132549.29 | 132346.63 | 129.47 |
| __bench_per_cpu_hash_map_update | Kernel | 68035.50 | 68140.89 | 68085.66 | 36.86 |
| __bench_per_cpu_hash_map_update | Userspace | 140907.26 | 141627.25 | 141286.33 | 267.26 |
| __bench_read | Kernel | 18016.83 | 18150.95 | 18083.10 | 38.69 |
| __bench_read | Userspace | 4683.82 | 4691.37 | 4686.99 | 2.30 |
| __bench_uprobe | Kernel | 1224.55 | 1341.71 | 1239.50 | 34.24 |
| __bench_uprobe | Userspace | 316.30 | 318.88 | 317.77 | 0.81 |
| __bench_uprobe_uretprobe | Kernel | 1745.81 | 1863.70 | 1764.21 | 33.48 |
| __bench_uprobe_uretprobe | Userspace | 333.09 | 334.63 | 333.69 | 0.54 |
| __bench_uretprobe | Kernel | 1635.26 | 1744.51 | 1651.89 | 31.39 |
| __bench_uretprobe | Userspace | 317.22 | 319.34 | 318.05 | 0.81 |
| __bench_write | Kernel | 17625.37 | 17872.57 | 17685.86 | 82.71 |
| __bench_write | Userspace | 4393.23 | 4969.03 | 4706.82 | 229.01 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 160.17 | 185.17 | 165.48 | 7.19 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-12 23:52:52
- **Total duration:** 4530.85 seconds

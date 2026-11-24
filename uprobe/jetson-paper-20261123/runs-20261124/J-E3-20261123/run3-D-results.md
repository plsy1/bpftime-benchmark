# BPFtime Uprobe Benchmark Results

*Generated on 2026-11-23 20:51:40*

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
| __bench_uprobe | 1243.47 | 210.79 | 5.90x |
| __bench_uretprobe | 1662.67 | 303.16 | 5.48x |
| __bench_uprobe_uretprobe | 1778.32 | 346.81 | 5.13x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3744.58 | 3758.89 | 3751.34 | 5.04 |
| __bench_array_map_delete | Userspace | 20296.98 | 20594.17 | 20372.63 | 101.40 |
| __bench_array_map_lookup | Kernel | 3791.56 | 3802.02 | 3797.12 | 4.00 |
| __bench_array_map_lookup | Userspace | 1180.53 | 1187.46 | 1185.11 | 1.95 |
| __bench_array_map_update | Kernel | 13111.49 | 13133.66 | 13122.37 | 8.35 |
| __bench_array_map_update | Userspace | 11506.15 | 11581.12 | 11535.22 | 24.92 |
| __bench_hash_map_delete | Kernel | 108686.44 | 109091.07 | 108809.90 | 138.87 |
| __bench_hash_map_delete | Userspace | 34066.53 | 35510.04 | 34859.27 | 566.05 |
| __bench_hash_map_lookup | Kernel | 28839.21 | 29170.77 | 29007.71 | 83.01 |
| __bench_hash_map_lookup | Userspace | 17569.74 | 17797.77 | 17662.20 | 57.08 |
| __bench_hash_map_update | Kernel | 95307.02 | 95493.90 | 95407.19 | 55.23 |
| __bench_hash_map_update | Userspace | 31158.81 | 32102.12 | 31662.17 | 330.69 |
| __bench_per_cpu_array_map_delete | Kernel | 3586.97 | 3604.83 | 3596.57 | 5.92 |
| __bench_per_cpu_array_map_delete | Userspace | 20297.34 | 20554.74 | 20380.97 | 90.88 |
| __bench_per_cpu_array_map_lookup | Kernel | 4298.03 | 4305.89 | 4301.81 | 2.61 |
| __bench_per_cpu_array_map_lookup | Userspace | 4115.07 | 4144.77 | 4129.86 | 8.75 |
| __bench_per_cpu_array_map_update | Kernel | 16574.02 | 16611.27 | 16590.27 | 10.39 |
| __bench_per_cpu_array_map_update | Userspace | 14987.76 | 16146.55 | 15160.46 | 332.24 |
| __bench_per_cpu_hash_map_delete | Kernel | 151500.35 | 151653.82 | 151594.84 | 49.10 |
| __bench_per_cpu_hash_map_delete | Userspace | 65543.77 | 69073.56 | 66830.12 | 1285.98 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29320.67 | 29465.91 | 29379.48 | 42.86 |
| __bench_per_cpu_hash_map_lookup | Userspace | 24644.15 | 24891.73 | 24732.99 | 67.06 |
| __bench_per_cpu_hash_map_update | Kernel | 68608.08 | 68661.94 | 68633.21 | 18.04 |
| __bench_per_cpu_hash_map_update | Userspace | 34365.34 | 35502.67 | 34819.07 | 446.02 |
| __bench_read | Kernel | 17542.94 | 17904.22 | 17634.67 | 129.16 |
| __bench_read | Userspace | 4582.04 | 4592.16 | 4587.52 | 2.90 |
| __bench_uprobe | Kernel | 1238.89 | 1254.65 | 1243.47 | 4.55 |
| __bench_uprobe | Userspace | 209.53 | 213.12 | 210.79 | 1.13 |
| __bench_uprobe_uretprobe | Kernel | 1768.37 | 1810.00 | 1778.32 | 11.81 |
| __bench_uprobe_uretprobe | Userspace | 345.39 | 353.43 | 346.81 | 2.32 |
| __bench_uretprobe | Kernel | 1657.36 | 1684.55 | 1662.67 | 7.55 |
| __bench_uretprobe | Userspace | 302.30 | 305.16 | 303.16 | 0.90 |
| __bench_write | Kernel | 17711.78 | 19283.84 | 17914.85 | 457.09 |
| __bench_write | Userspace | 4284.93 | 4289.12 | 4286.75 | 1.39 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.58 | 161.31 | 156.09 | 2.68 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-11-23 20:51:40
- **Total duration:** 1290.04 seconds

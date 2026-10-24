# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-16 19:14:03*

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
| __bench_uprobe | 1229.66 | 196.64 | 6.25x |
| __bench_uretprobe | 1645.46 | 268.89 | 6.12x |
| __bench_uprobe_uretprobe | 1754.75 | 317.25 | 5.53x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3675.87 | 3684.12 | 3678.25 | 2.14 |
| __bench_array_map_delete | Userspace | 19974.98 | 21540.29 | 20241.75 | 441.92 |
| __bench_array_map_lookup | Kernel | 3737.80 | 3746.42 | 3740.91 | 2.91 |
| __bench_array_map_lookup | Userspace | 10872.46 | 10917.36 | 10894.31 | 13.12 |
| __bench_array_map_update | Kernel | 13204.75 | 13241.86 | 13218.22 | 11.05 |
| __bench_array_map_update | Userspace | 13886.27 | 14259.47 | 13968.20 | 102.03 |
| __bench_hash_map_delete | Kernel | 107902.60 | 107999.30 | 107934.90 | 27.24 |
| __bench_hash_map_delete | Userspace | 33852.21 | 40365.17 | 34965.48 | 2005.58 |
| __bench_hash_map_lookup | Kernel | 28686.46 | 29108.60 | 28894.79 | 117.10 |
| __bench_hash_map_lookup | Userspace | 28177.89 | 31038.12 | 28583.71 | 822.17 |
| __bench_hash_map_update | Kernel | 92828.98 | 92958.77 | 92873.40 | 36.31 |
| __bench_hash_map_update | Userspace | 43817.01 | 44724.52 | 44218.70 | 294.11 |
| __bench_per_cpu_array_map_delete | Kernel | 3683.17 | 3695.77 | 3688.35 | 4.00 |
| __bench_per_cpu_array_map_delete | Userspace | 19984.63 | 21419.19 | 20199.06 | 409.67 |
| __bench_per_cpu_array_map_lookup | Kernel | 4404.81 | 4412.79 | 4407.42 | 2.72 |
| __bench_per_cpu_array_map_lookup | Userspace | 14649.91 | 14772.98 | 14720.24 | 40.11 |
| __bench_per_cpu_array_map_update | Kernel | 16618.20 | 16685.73 | 16650.75 | 18.10 |
| __bench_per_cpu_array_map_update | Userspace | 27726.73 | 28123.90 | 27880.96 | 104.08 |
| __bench_per_cpu_hash_map_delete | Kernel | 151500.49 | 151680.08 | 151578.75 | 58.38 |
| __bench_per_cpu_hash_map_delete | Userspace | 98615.22 | 99917.72 | 99031.08 | 396.70 |
| __bench_per_cpu_hash_map_lookup | Kernel | 28485.93 | 28726.87 | 28627.44 | 64.25 |
| __bench_per_cpu_hash_map_lookup | Userspace | 44892.74 | 46351.00 | 45106.60 | 418.05 |
| __bench_per_cpu_hash_map_update | Kernel | 67817.87 | 67898.35 | 67856.77 | 25.66 |
| __bench_per_cpu_hash_map_update | Userspace | 61320.23 | 63993.02 | 61776.19 | 752.88 |
| __bench_read | Kernel | 18087.05 | 18220.58 | 18133.37 | 47.91 |
| __bench_read | Userspace | 4556.92 | 4563.08 | 4559.91 | 1.71 |
| __bench_uprobe | Kernel | 1225.66 | 1235.73 | 1229.66 | 3.32 |
| __bench_uprobe | Userspace | 194.48 | 198.54 | 196.64 | 1.22 |
| __bench_uprobe_uretprobe | Kernel | 1746.96 | 1764.17 | 1754.75 | 5.75 |
| __bench_uprobe_uretprobe | Userspace | 316.34 | 318.87 | 317.25 | 0.78 |
| __bench_uretprobe | Kernel | 1638.64 | 1660.57 | 1645.46 | 6.61 |
| __bench_uretprobe | Userspace | 267.30 | 270.94 | 268.89 | 1.30 |
| __bench_write | Kernel | 17587.90 | 17721.77 | 17683.48 | 35.26 |
| __bench_write | Userspace | 4268.81 | 4272.09 | 4270.36 | 0.88 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 152.88 | 184.21 | 158.56 | 8.89 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-16 19:14:03
- **Total duration:** 3924.21 seconds

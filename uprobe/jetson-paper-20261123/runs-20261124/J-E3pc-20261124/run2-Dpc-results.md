# BPFtime Uprobe Benchmark Results

*Generated on 2026-11-24 05:23:15*

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
| __bench_uprobe | 1249.90 | 210.08 | 5.95x |
| __bench_uretprobe | 1668.15 | 302.05 | 5.52x |
| __bench_uprobe_uretprobe | 1774.29 | 346.59 | 5.12x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3744.29 | 3807.89 | 3756.32 | 18.34 |
| __bench_array_map_delete | Userspace | 19822.13 | 20062.52 | 19892.72 | 84.31 |
| __bench_array_map_lookup | Kernel | 3795.18 | 3824.40 | 3803.56 | 8.67 |
| __bench_array_map_lookup | Userspace | 1184.41 | 1197.46 | 1187.62 | 3.86 |
| __bench_array_map_update | Kernel | 13112.80 | 13162.13 | 13127.32 | 12.65 |
| __bench_array_map_update | Userspace | 11529.09 | 11632.51 | 11573.05 | 26.49 |
| __bench_hash_map_delete | Kernel | 108834.43 | 109028.23 | 108931.91 | 66.11 |
| __bench_hash_map_delete | Userspace | 34508.05 | 35918.16 | 35267.94 | 578.56 |
| __bench_hash_map_lookup | Kernel | 28784.24 | 29158.12 | 28915.87 | 117.10 |
| __bench_hash_map_lookup | Userspace | 17943.94 | 18060.28 | 18003.84 | 36.85 |
| __bench_hash_map_update | Kernel | 95001.62 | 96725.89 | 95275.86 | 490.65 |
| __bench_hash_map_update | Userspace | 31409.02 | 32454.10 | 31953.84 | 329.47 |
| __bench_per_cpu_array_map_delete | Kernel | 3744.39 | 3767.70 | 3751.14 | 6.71 |
| __bench_per_cpu_array_map_delete | Userspace | 19880.76 | 20162.43 | 19968.80 | 96.58 |
| __bench_per_cpu_array_map_lookup | Kernel | 4522.92 | 4542.73 | 4530.47 | 7.22 |
| __bench_per_cpu_array_map_lookup | Userspace | 4119.72 | 4147.88 | 4130.95 | 8.86 |
| __bench_per_cpu_array_map_update | Kernel | 16667.00 | 16714.75 | 16692.74 | 12.92 |
| __bench_per_cpu_array_map_update | Userspace | 14896.81 | 15210.80 | 14997.79 | 87.00 |
| __bench_per_cpu_hash_map_delete | Kernel | 150496.53 | 151448.29 | 150673.29 | 264.22 |
| __bench_per_cpu_hash_map_delete | Userspace | 65600.57 | 68149.87 | 66848.50 | 1003.47 |
| __bench_per_cpu_hash_map_lookup | Kernel | 29202.33 | 29343.79 | 29261.18 | 47.63 |
| __bench_per_cpu_hash_map_lookup | Userspace | 24547.37 | 24746.38 | 24636.42 | 58.41 |
| __bench_per_cpu_hash_map_update | Kernel | 67595.53 | 67718.63 | 67655.60 | 31.04 |
| __bench_per_cpu_hash_map_update | Userspace | 34285.85 | 35345.91 | 34794.30 | 423.80 |
| __bench_read | Kernel | 17556.32 | 17690.36 | 17626.84 | 37.47 |
| __bench_read | Userspace | 859127.76 | 904968.39 | 886939.51 | 15467.91 |
| __bench_uprobe | Kernel | 1238.31 | 1301.28 | 1249.90 | 18.04 |
| __bench_uprobe | Userspace | 207.05 | 211.93 | 210.08 | 1.62 |
| __bench_uprobe_uretprobe | Kernel | 1767.63 | 1794.88 | 1774.29 | 7.49 |
| __bench_uprobe_uretprobe | Userspace | 345.38 | 348.39 | 346.59 | 0.79 |
| __bench_uretprobe | Kernel | 1658.63 | 1721.60 | 1668.15 | 18.02 |
| __bench_uretprobe | Userspace | 300.18 | 304.12 | 302.05 | 1.20 |
| __bench_write | Kernel | 17804.93 | 17867.34 | 17839.19 | 17.84 |
| __bench_write | Userspace | 868358.80 | 939934.36 | 896565.90 | 21616.27 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 154.50 | 179.91 | 160.47 | 7.22 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-11-24 05:23:15
- **Total duration:** 3063.58 seconds

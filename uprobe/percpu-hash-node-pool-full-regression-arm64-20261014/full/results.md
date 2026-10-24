# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-14 14:02:14*

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
| __bench_uprobe | 1228.73 | 318.92 | 3.85x |
| __bench_uretprobe | 1644.83 | 318.59 | 5.16x |
| __bench_uprobe_uretprobe | 1760.28 | 333.51 | 5.28x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3517.25 | 3561.83 | 3526.31 | 12.29 |
| __bench_array_map_delete | Userspace | 16229.59 | 16371.86 | 16291.33 | 37.97 |
| __bench_array_map_lookup | Kernel | 3783.58 | 3793.94 | 3788.51 | 3.91 |
| __bench_array_map_lookup | Userspace | 15748.23 | 15956.30 | 15865.79 | 75.59 |
| __bench_array_map_update | Kernel | 12994.17 | 13034.34 | 13018.22 | 13.54 |
| __bench_array_map_update | Userspace | 23346.91 | 23608.24 | 23485.23 | 101.66 |
| __bench_hash_map_delete | Kernel | 108002.95 | 108143.58 | 108045.44 | 43.02 |
| __bench_hash_map_delete | Userspace | 41367.48 | 43113.00 | 42394.23 | 723.01 |
| __bench_hash_map_lookup | Kernel | 27934.64 | 28204.45 | 28081.77 | 82.99 |
| __bench_hash_map_lookup | Userspace | 56145.10 | 58541.87 | 57291.78 | 923.39 |
| __bench_hash_map_update | Kernel | 93619.31 | 93708.93 | 93662.73 | 33.60 |
| __bench_hash_map_update | Userspace | 61860.05 | 63563.78 | 62651.38 | 714.08 |
| __bench_per_cpu_array_map_delete | Kernel | 3517.19 | 3525.46 | 3520.41 | 2.95 |
| __bench_per_cpu_array_map_delete | Userspace | 16091.48 | 16316.56 | 16246.26 | 72.65 |
| __bench_per_cpu_array_map_lookup | Kernel | 4215.04 | 4230.70 | 4221.21 | 5.63 |
| __bench_per_cpu_array_map_lookup | Userspace | 21175.60 | 21567.78 | 21341.24 | 117.58 |
| __bench_per_cpu_array_map_update | Kernel | 16622.51 | 16667.74 | 16641.32 | 13.22 |
| __bench_per_cpu_array_map_update | Userspace | 26913.40 | 27206.76 | 27087.14 | 100.37 |
| __bench_per_cpu_hash_map_delete | Kernel | 151988.61 | 152364.10 | 152153.81 | 86.85 |
| __bench_per_cpu_hash_map_delete | Userspace | 109539.84 | 109861.56 | 109645.98 | 120.18 |
| __bench_per_cpu_hash_map_lookup | Kernel | 28681.63 | 28795.62 | 28721.77 | 33.13 |
| __bench_per_cpu_hash_map_lookup | Userspace | 61892.79 | 62302.33 | 62094.96 | 124.82 |
| __bench_per_cpu_hash_map_update | Kernel | 67841.80 | 68322.71 | 67999.35 | 127.06 |
| __bench_per_cpu_hash_map_update | Userspace | 72378.04 | 72651.22 | 72552.41 | 74.67 |
| __bench_read | Kernel | 18059.25 | 18164.42 | 18096.73 | 34.38 |
| __bench_read | Userspace | 4683.54 | 4699.07 | 4690.58 | 4.61 |
| __bench_uprobe | Kernel | 1224.41 | 1240.03 | 1228.73 | 4.39 |
| __bench_uprobe | Userspace | 316.11 | 321.12 | 318.92 | 1.38 |
| __bench_uprobe_uretprobe | Kernel | 1751.49 | 1773.83 | 1760.28 | 6.66 |
| __bench_uprobe_uretprobe | Userspace | 331.48 | 336.04 | 333.51 | 1.35 |
| __bench_uretprobe | Kernel | 1639.59 | 1656.64 | 1644.83 | 4.92 |
| __bench_uretprobe | Userspace | 315.98 | 320.87 | 318.59 | 1.48 |
| __bench_write | Kernel | 17654.12 | 17732.86 | 17690.51 | 27.49 |
| __bench_write | Userspace | 4395.20 | 4400.51 | 4397.85 | 1.72 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 158.44 | 185.64 | 163.14 | 7.77 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-14 14:02:14
- **Total duration:** 1679.85 seconds

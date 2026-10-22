# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-22 17:17:54*

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

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Userspace | 20111.97 | 20382.57 | 20225.61 | 89.34 |
| __bench_array_map_lookup | Userspace | 1170.29 | 1177.23 | 1173.12 | 1.71 |
| __bench_array_map_update | Userspace | 11503.44 | 11584.11 | 11536.46 | 27.29 |
| __bench_hash_map_delete | Userspace | 36596.10 | 37266.42 | 36863.64 | 243.22 |
| __bench_hash_map_lookup | Userspace | 23729.17 | 24926.56 | 24280.76 | 525.38 |
| __bench_hash_map_update | Userspace | 27354.70 | 31523.69 | 29437.63 | 1865.66 |
| __bench_per_cpu_array_map_delete | Userspace | 20180.10 | 20480.84 | 20335.52 | 92.68 |
| __bench_per_cpu_array_map_lookup | Userspace | 4102.72 | 4132.66 | 4116.54 | 10.10 |
| __bench_per_cpu_array_map_update | Userspace | 14417.41 | 14642.69 | 14508.77 | 55.94 |
| __bench_per_cpu_hash_map_delete | Userspace | 98491.44 | 99426.19 | 98864.93 | 323.12 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27633.65 | 27858.89 | 27739.92 | 76.61 |
| __bench_per_cpu_hash_map_update | Userspace | 44373.84 | 44681.37 | 44463.48 | 79.43 |
| __bench_read | Userspace | 4569.75 | 4574.33 | 4572.09 | 1.65 |
| __bench_uprobe | Userspace | 197.50 | 202.60 | 199.59 | 1.80 |
| __bench_uprobe_uretprobe | Userspace | 355.44 | 363.56 | 358.08 | 2.49 |
| __bench_uretprobe | Userspace | 268.35 | 273.29 | 270.03 | 1.35 |
| __bench_write | Userspace | 4273.74 | 4851.94 | 4547.04 | 245.85 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 153.28 | 172.02 | 157.17 | 5.38 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-22 17:17:54
- **Total duration:** 531.35 seconds

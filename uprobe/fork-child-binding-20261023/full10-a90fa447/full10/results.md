# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-23 15:15:24*

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
| __bench_array_map_delete | Userspace | 20795.23 | 20973.65 | 20869.84 | 74.67 |
| __bench_array_map_lookup | Userspace | 1163.33 | 1173.38 | 1168.68 | 3.08 |
| __bench_array_map_update | Userspace | 11480.53 | 11592.88 | 11526.09 | 40.12 |
| __bench_hash_map_delete | Userspace | 27139.38 | 28337.74 | 27361.84 | 334.37 |
| __bench_hash_map_lookup | Userspace | 23849.08 | 24962.44 | 24749.98 | 306.03 |
| __bench_hash_map_update | Userspace | 27514.97 | 31480.52 | 30740.01 | 1119.21 |
| __bench_per_cpu_array_map_delete | Userspace | 20790.87 | 21005.62 | 20888.38 | 76.99 |
| __bench_per_cpu_array_map_lookup | Userspace | 4103.87 | 4117.12 | 4109.22 | 4.29 |
| __bench_per_cpu_array_map_update | Userspace | 14399.60 | 14514.21 | 14456.36 | 38.37 |
| __bench_per_cpu_hash_map_delete | Userspace | 98784.91 | 108684.12 | 100035.39 | 2894.89 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27540.94 | 27829.09 | 27680.84 | 101.67 |
| __bench_per_cpu_hash_map_update | Userspace | 44353.43 | 44528.75 | 44490.95 | 47.52 |
| __bench_read | Userspace | 4570.10 | 4587.70 | 4580.02 | 5.65 |
| __bench_uprobe | Userspace | 197.58 | 199.10 | 198.33 | 0.50 |
| __bench_uprobe_uretprobe | Userspace | 323.15 | 332.76 | 324.77 | 2.72 |
| __bench_uretprobe | Userspace | 269.78 | 271.88 | 270.49 | 0.60 |
| __bench_write | Userspace | 4272.17 | 4285.30 | 4275.23 | 3.61 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 155.79 | 179.58 | 160.35 | 6.95 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-23 15:15:24
- **Total duration:** 526.13 seconds

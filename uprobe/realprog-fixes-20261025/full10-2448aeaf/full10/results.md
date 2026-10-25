# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-25 19:53:18*

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
| __bench_array_map_delete | Userspace | 20818.51 | 21009.13 | 20880.93 | 65.19 |
| __bench_array_map_lookup | Userspace | 1187.61 | 1202.75 | 1195.31 | 4.49 |
| __bench_array_map_update | Userspace | 11491.32 | 11714.66 | 11537.15 | 61.79 |
| __bench_hash_map_delete | Userspace | 27154.48 | 28495.46 | 27878.87 | 602.10 |
| __bench_hash_map_lookup | Userspace | 22873.96 | 23679.67 | 23204.80 | 282.08 |
| __bench_hash_map_update | Userspace | 26614.67 | 28505.23 | 27647.76 | 781.40 |
| __bench_per_cpu_array_map_delete | Userspace | 20804.15 | 20979.40 | 20862.90 | 59.72 |
| __bench_per_cpu_array_map_lookup | Userspace | 4103.72 | 4136.91 | 4116.65 | 9.97 |
| __bench_per_cpu_array_map_update | Userspace | 14341.98 | 14700.06 | 14432.60 | 98.53 |
| __bench_per_cpu_hash_map_delete | Userspace | 98785.55 | 98943.04 | 98867.70 | 50.84 |
| __bench_per_cpu_hash_map_lookup | Userspace | 27605.73 | 27818.22 | 27706.55 | 54.69 |
| __bench_per_cpu_hash_map_update | Userspace | 44298.34 | 44493.12 | 44408.68 | 63.46 |
| __bench_read | Userspace | 4577.71 | 4584.38 | 4580.43 | 2.16 |
| __bench_uprobe | Userspace | 197.44 | 200.65 | 199.23 | 1.05 |
| __bench_uprobe_uretprobe | Userspace | 322.57 | 328.35 | 324.39 | 1.56 |
| __bench_uretprobe | Userspace | 265.37 | 271.97 | 267.66 | 1.87 |
| __bench_write | Userspace | 4273.16 | 4276.07 | 4274.65 | 0.82 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 153.80 | 183.29 | 160.44 | 8.05 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-25 19:53:18
- **Total duration:** 512.25 seconds

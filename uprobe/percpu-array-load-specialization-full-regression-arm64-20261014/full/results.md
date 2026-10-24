# BPFtime Uprobe Benchmark Results

*Generated on 2026-10-14 16:33:23*

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
| __bench_uprobe | 1229.14 | 317.89 | 3.87x |
| __bench_uretprobe | 1662.48 | 318.69 | 5.22x |
| __bench_uprobe_uretprobe | 1775.71 | 335.78 | 5.29x |

### Kernel vs Userspace eBPF Detailed Comparison

| Operation | Environment | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|-------------|----------|----------|----------|---------|
| __bench_array_map_delete | Kernel | 3537.47 | 3550.84 | 3543.51 | 3.61 |
| __bench_array_map_delete | Userspace | 16217.37 | 16324.82 | 16269.63 | 36.83 |
| __bench_array_map_lookup | Kernel | 3675.65 | 3684.92 | 3680.79 | 3.15 |
| __bench_array_map_lookup | Userspace | 15701.65 | 15930.56 | 15808.59 | 60.69 |
| __bench_array_map_update | Kernel | 13013.45 | 13043.23 | 13027.58 | 9.78 |
| __bench_array_map_update | Userspace | 23357.91 | 23654.28 | 23497.50 | 98.81 |
| __bench_hash_map_delete | Kernel | 108696.52 | 108974.33 | 108836.84 | 104.05 |
| __bench_hash_map_delete | Userspace | 41370.91 | 43088.07 | 42136.82 | 556.53 |
| __bench_hash_map_lookup | Kernel | 28489.43 | 28800.46 | 28596.46 | 101.97 |
| __bench_hash_map_lookup | Userspace | 56161.14 | 58134.50 | 56865.11 | 759.77 |
| __bench_hash_map_update | Kernel | 94016.41 | 94207.26 | 94103.54 | 53.13 |
| __bench_hash_map_update | Userspace | 61531.41 | 63234.67 | 62492.24 | 752.40 |
| __bench_per_cpu_array_map_delete | Kernel | 3691.67 | 3700.50 | 3695.71 | 3.09 |
| __bench_per_cpu_array_map_delete | Userspace | 16101.09 | 16320.34 | 16231.95 | 69.89 |
| __bench_per_cpu_array_map_lookup | Kernel | 4398.71 | 4401.46 | 4400.32 | 0.84 |
| __bench_per_cpu_array_map_lookup | Userspace | 12797.20 | 12960.51 | 12907.88 | 44.78 |
| __bench_per_cpu_array_map_update | Kernel | 16595.81 | 16640.97 | 16618.26 | 15.29 |
| __bench_per_cpu_array_map_update | Userspace | 26964.87 | 27253.87 | 27134.44 | 76.93 |
| __bench_per_cpu_hash_map_delete | Kernel | 151129.82 | 151384.01 | 151275.75 | 64.88 |
| __bench_per_cpu_hash_map_delete | Userspace | 109401.16 | 109790.98 | 109594.58 | 127.23 |
| __bench_per_cpu_hash_map_lookup | Kernel | 28640.41 | 28764.12 | 28714.14 | 43.47 |
| __bench_per_cpu_hash_map_lookup | Userspace | 61598.86 | 61976.04 | 61806.20 | 129.11 |
| __bench_per_cpu_hash_map_update | Kernel | 67284.29 | 67367.38 | 67327.47 | 30.15 |
| __bench_per_cpu_hash_map_update | Userspace | 71947.64 | 72291.97 | 72167.67 | 106.18 |
| __bench_read | Kernel | 17621.61 | 17723.16 | 17664.26 | 30.32 |
| __bench_read | Userspace | 4681.48 | 4687.99 | 4683.90 | 1.88 |
| __bench_uprobe | Kernel | 1224.13 | 1237.30 | 1229.14 | 4.42 |
| __bench_uprobe | Userspace | 315.99 | 322.22 | 317.89 | 1.82 |
| __bench_uprobe_uretprobe | Kernel | 1748.39 | 1810.87 | 1775.71 | 15.45 |
| __bench_uprobe_uretprobe | Userspace | 334.15 | 337.41 | 335.78 | 1.09 |
| __bench_uretprobe | Kernel | 1641.26 | 1676.30 | 1662.48 | 10.41 |
| __bench_uretprobe | Userspace | 316.55 | 324.00 | 318.69 | 2.15 |
| __bench_write | Kernel | 17741.17 | 17819.17 | 17781.32 | 27.42 |
| __bench_write | Userspace | 4391.33 | 4395.72 | 4393.80 | 1.29 |

### Embedded VM Performance

| Operation | Min (ns) | Max (ns) | Avg (ns) | Std Dev |
|-----------|----------|----------|----------|---------|
| embed | 159.18 | 182.08 | 162.84 | 6.46 |

## Benchmark Metadata

- **Number of runs:** 10
- **Timestamp:** 2026-10-14 16:33:23
- **Total duration:** 1668.81 seconds

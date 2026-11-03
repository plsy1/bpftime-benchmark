| case | 7ad5a152 | x bpf | d5279469 | x bpf | change (mean) | change (median) |
|---|---:|---:|---:|---:|---:|---:|
| uprobe_uretprobe | 214.13 | 0.16x | 216.37 | 0.16x | -1.0% | -1.1% |
| uretprobe | 183.11 | 0.14x | 184.81 | 0.14x | -0.9% | -0.9% |
| uprobe | 123.67 | 0.11x | 123.35 | 0.11x | +0.3% | +0.2% |
| read | 1507.65 | 0.08x | 1506.37 | 0.08x | +0.1% | +0.1% |
| write | 1282.75 | 0.07x | 1279.72 | 0.07x | +0.2% | +0.3% |
| hash_map_update | 10.92 | 0.32x | 11.78 | 0.34x | -7.3% | -10.1% |
| hash_map_lookup | 5.32 | 0.50x | 4.94 | 0.46x | +7.7% | -3.2% |
| hash_map_delete | 15.04 | 0.44x | 14.61 | 0.43x | +3.0% | -0.5% |
| array_map_update | 3.77 | 0.51x | 3.37 | 0.45x | +11.7% | +2.2% |
| array_map_lookup | 0.45 | 0.25x | 0.45 | 0.24x | +0.3% | -0.1% |
| array_map_delete | 5.70 | 3.27x | 5.41 | 3.11x | +5.4% | +5.4% |
| per_cpu_hash_map_update | 12.46 | 0.49x | 11.87 | 0.46x | +5.0% | +0.6% |
| per_cpu_hash_map_lookup | 7.66 | 0.68x | 6.97 | 0.66x | +9.8% | +2.2% |
| per_cpu_hash_map_delete | 26.02 | 0.23x | 25.62 | 0.23x | +1.6% | +1.0% |
| per_cpu_array_map_update | 5.16 | 0.68x | 4.84 | 0.65x | +6.5% | -0.2% |
| per_cpu_array_map_lookup | 1.28 | 0.70x | 1.28 | 0.70x | -0.1% | -0.1% |
| per_cpu_array_map_delete | 5.72 | 3.27x | 5.41 | 3.06x | +5.6% | +5.3% |

embed: 7ad5a152 63.10 ns, d5279469 63.17 ns (-0.1%)

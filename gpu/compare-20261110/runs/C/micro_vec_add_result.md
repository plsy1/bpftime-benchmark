
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T12:07:42.287090  

## Workload Configuration

| Workload | Binary | Elements | Iterations | Threads | Blocks |
|----------|--------|----------|------------|---------|--------|
| large | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add | 100000 | 1000 | 512 | 196 |
| medium | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add | 10000 | 10000 | 256 | 40 |
| minimal | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add | 32 | 3 | 32 | 1 |
| small | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add | 1000 | 10000 | 256 | 4 |
| small_64x16 | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add | 1000 | 10000 | 64 | 16 |
| tiny | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add | 32 | 10000 | 32 | 1 |
| xlarge | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add | 1000000 | 1000 | 512 | 1954 |

## Benchmark Results

| Test Name | Workload | Avg Time (μs) | vs Baseline | Overhead |
|-----------|----------|---------------|-------------|----------|
| Baseline (tiny) | tiny | 27.99 | - | - |
| Baseline (small) | small | 27.46 | - | - |
| Baseline (medium) | medium | 26.72 | - | - |
| Baseline (large) | large | 31.40 | - | - |
| Baseline (xlarge) | xlarge | 158.87 | - | - |
| Baseline (minimal) | minimal | 39.14 | - | - |
| Empty probe (tiny) | tiny | 32.61 | 27.99 | 1.17x (+16.5%) |
| Empty probe (small) | small | 36.01 | 27.46 | 1.31x (+31.1%) |
| Empty probe (medium) | medium | 38.83 | 26.72 | 1.45x (+45.3%) |
| Empty probe (large) | large | 41.30 | 31.40 | 1.32x (+31.5%) |
| Empty probe (xlarge) | xlarge | 172.43 | 158.87 | 1.09x (+8.5%) |
| Entry probe (tiny) | tiny | 34.95 | 27.99 | 1.25x (+24.9%) |
| Entry probe (small) | small | 33.22 | 27.46 | 1.21x (+21.0%) |
| Entry probe (medium) | medium | 36.76 | 26.72 | 1.38x (+37.6%) |
| Entry probe (xlarge) | xlarge | 167.42 | 158.87 | 1.05x (+5.4%) |
| Exit probe (tiny) | tiny | 35.75 | 27.99 | 1.28x (+27.7%) |
| Exit probe (small) | small | 34.66 | 27.46 | 1.26x (+26.2%) |
| Exit probe (large) | large | 38.01 | 31.40 | 1.21x (+21.1%) |
| Entry+Exit (tiny) | tiny | 35.06 | 27.99 | 1.25x (+25.3%) |
| Entry+Exit (small_64x16) | small_64x16 | 33.37 | 27.46 | 1.22x (+21.5%) |
| Entry+Exit (small) | small | 31.52 | 27.46 | 1.15x (+14.8%) |
| Entry+Exit (medium) | medium | 34.27 | 26.72 | 1.28x (+28.3%) |
| Entry+Exit (large) | large | 40.65 | 31.40 | 1.29x (+29.5%) |
| Entry+Exit (xlarge) | xlarge | 173.81 | 158.87 | 1.09x (+9.4%) |
| GPU Ringbuf (tiny) | tiny | 36.98 | 27.99 | 1.32x (+32.1%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 47.67 | 27.46 | 1.74x (+73.6%) |
| GPU Ringbuf (small) | small | 42.96 | 27.46 | 1.56x (+56.4%) |
| GPU Ringbuf (medium) | medium | 45.38 | 26.72 | 1.70x (+69.8%) |
| GPU Ringbuf (large) | large | 50.41 | 31.40 | 1.61x (+60.5%) |
| GPU Ringbuf (xlarge) | xlarge | 215.20 | 158.87 | 1.35x (+35.5%) |
| Global timer (tiny) | tiny | 31.21 | 27.99 | 1.12x (+11.5%) |
| Global timer (small) | small | 37.40 | 27.46 | 1.36x (+36.2%) |
| Global timer (medium) | medium | 30.55 | 26.72 | 1.14x (+14.3%) |
| Global timer (xlarge) | xlarge | 179.63 | 158.87 | 1.13x (+13.1%) |
| Per-GPU-thread array (tiny) | tiny | 35.13 | 27.99 | 1.26x (+25.5%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 39.22 | 27.46 | 1.43x (+42.8%) |
| Per-GPU-thread array (small) | small | 37.15 | 27.46 | 1.35x (+35.3%) |
| Per-GPU-thread array (medium) | medium | 45.67 | 26.72 | 1.71x (+70.9%) |
| Per-GPU-thread array (large) | large | 146.60 | 31.40 | 4.67x (+366.9%) |
| Per-GPU-thread array (xlarge) | xlarge | 1171.42 | 158.87 | 7.37x (+637.3%) |
| Memtrace (tiny) | tiny | 38.36 | 27.99 | 1.37x (+37.0%) |
| Memtrace (small) | small | 38.47 | 27.46 | 1.40x (+40.1%) |
| Memtrace (medium) | medium | 47.75 | 26.72 | 1.79x (+78.7%) |
| Memtrace (large) | large | 173.05 | 31.40 | 5.51x (+451.1%) |
| GPU Array map update (tiny) | tiny | 36.33 | 27.99 | 1.30x (+29.8%) |
| GPU Array map update (small_64x16) | small_64x16 | 34.80 | 27.46 | 1.27x (+26.7%) |
| GPU Array map update (small) | small | 39.02 | 27.46 | 1.42x (+42.1%) |
| GPU Array map update (medium) | medium | 46.72 | 26.72 | 1.75x (+74.9%) |
| GPU Array map update (large) | large | 251.44 | 31.40 | 8.01x (+700.8%) |
| GPU Array map update (xlarge) | xlarge | 1455.75 | 158.87 | 9.16x (+816.3%) |
| GPU Array map lookup (tiny) | tiny | 37.89 | 27.99 | 1.35x (+35.4%) |
| GPU Array map lookup (small_64x16) | small_64x16 | 37.63 | 27.46 | 1.37x (+37.0%) |
| GPU Array map lookup (small) | small | 37.57 | 27.46 | 1.37x (+36.8%) |
| GPU Array map lookup (medium) | medium | 34.36 | 26.72 | 1.29x (+28.6%) |
| GPU Array map lookup (large) | large | 63.32 | 31.40 | 2.02x (+101.7%) |
| GPU Array map lookup (xlarge) | xlarge | 445.05 | 158.87 | 2.80x (+180.1%) |
| CPU Array map update (minimal) | minimal | 316.25 | 27.99 | 11.30x (+1029.9%) |
| CPU Array map lookup (minimal) | minimal | 375.84 | 27.99 | 13.43x (+1242.8%) |
| CPU Hash map update (minimal) | minimal | 315.84 | 27.99 | 11.28x (+1028.4%) |
| CPU Hash map lookup (minimal) | minimal | 337.62 | 27.99 | 12.06x (+1106.2%) |
| CPU Hash map delete (minimal) | minimal | 327.21 | 27.99 | 11.69x (+1069.0%) |



# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T11:10:14.987684  

## Workload Configuration

| Workload | Binary | Elements | Iterations | Threads | Blocks |
|----------|--------|----------|------------|---------|--------|
| large | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add_legacy | 100000 | 1000 | 512 | 196 |
| medium | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add_legacy | 10000 | 10000 | 256 | 40 |
| minimal | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add_legacy | 32 | 3 | 32 | 1 |
| small | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add_legacy | 1000 | 10000 | 256 | 4 |
| small_64x16 | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add_legacy | 1000 | 10000 | 64 | 16 |
| tiny | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add_legacy | 32 | 10000 | 32 | 1 |
| xlarge | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add_legacy | 1000000 | 1000 | 512 | 1954 |

## Benchmark Results

| Test Name | Workload | Avg Time (μs) | vs Baseline | Overhead |
|-----------|----------|---------------|-------------|----------|
| Baseline (tiny) | tiny | 27.52 | - | - |
| Baseline (small) | small | 28.54 | - | - |
| Baseline (medium) | medium | 27.80 | - | - |
| Baseline (large) | large | 31.59 | - | - |
| Baseline (xlarge) | xlarge | 159.99 | - | - |
| Baseline (minimal) | minimal | 41.89 | - | - |
| Empty probe (tiny) | tiny | 31.49 | 27.52 | 1.14x (+14.4%) |
| Empty probe (small) | small | 35.88 | 28.54 | 1.26x (+25.7%) |
| Empty probe (medium) | medium | 31.97 | 27.80 | 1.15x (+15.0%) |
| Empty probe (large) | large | 39.47 | 31.59 | 1.25x (+24.9%) |
| Empty probe (xlarge) | xlarge | 169.56 | 159.99 | 1.06x (+6.0%) |
| Entry probe (tiny) | tiny | 32.67 | 27.52 | 1.19x (+18.7%) |
| Entry probe (small) | small | 33.78 | 28.54 | 1.18x (+18.4%) |
| Entry probe (medium) | medium | 33.90 | 27.80 | 1.22x (+21.9%) |
| Entry probe (xlarge) | xlarge | 172.95 | 159.99 | 1.08x (+8.1%) |
| Exit probe (tiny) | tiny | 33.80 | 27.52 | 1.23x (+22.8%) |
| Exit probe (small) | small | 36.71 | 28.54 | 1.29x (+28.6%) |
| Exit probe (large) | large | 42.15 | 31.59 | 1.33x (+33.4%) |
| Entry+Exit (tiny) | tiny | 31.65 | 27.52 | 1.15x (+15.0%) |
| Entry+Exit (small_64x16) | small_64x16 | 35.61 | 28.54 | 1.25x (+24.8%) |
| Entry+Exit (small) | small | 35.38 | 28.54 | 1.24x (+24.0%) |
| Entry+Exit (medium) | medium | 34.99 | 27.80 | 1.26x (+25.9%) |
| Entry+Exit (large) | large | 37.04 | 31.59 | 1.17x (+17.3%) |
| Entry+Exit (xlarge) | xlarge | 170.80 | 159.99 | 1.07x (+6.8%) |
| GPU Ringbuf (tiny) | tiny | 34.40 | 27.52 | 1.25x (+25.0%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 46.52 | 28.54 | 1.63x (+63.0%) |
| GPU Ringbuf (small) | small | 44.51 | 28.54 | 1.56x (+56.0%) |
| GPU Ringbuf (medium) | medium | 48.16 | 27.80 | 1.73x (+73.2%) |
| GPU Ringbuf (large) | large | 51.20 | 31.59 | 1.62x (+62.1%) |
| GPU Ringbuf (xlarge) | xlarge | 214.41 | 159.99 | 1.34x (+34.0%) |
| Global timer (tiny) | tiny | 35.30 | 27.52 | 1.28x (+28.3%) |
| Global timer (small) | small | 37.10 | 28.54 | 1.30x (+30.0%) |
| Global timer (medium) | medium | 37.37 | 27.80 | 1.34x (+34.4%) |
| Global timer (xlarge) | xlarge | 176.63 | 159.99 | 1.10x (+10.4%) |
| Per-GPU-thread array (tiny) | tiny | 32.05 | 27.52 | 1.16x (+16.5%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 35.11 | 28.54 | 1.23x (+23.0%) |
| Per-GPU-thread array (small) | small | 39.57 | 28.54 | 1.39x (+38.6%) |
| Per-GPU-thread array (medium) | medium | 43.94 | 27.80 | 1.58x (+58.1%) |
| Per-GPU-thread array (large) | large | 147.42 | 31.59 | 4.67x (+366.7%) |
| Per-GPU-thread array (xlarge) | xlarge | 1166.50 | 159.99 | 7.29x (+629.1%) |
| Memtrace (tiny) | tiny | 36.34 | 27.52 | 1.32x (+32.0%) |
| Memtrace (small) | small | 36.54 | 28.54 | 1.28x (+28.0%) |
| Memtrace (medium) | medium | 43.47 | 27.80 | 1.56x (+56.4%) |
| Memtrace (large) | large | 174.56 | 31.59 | 5.53x (+452.6%) |
| GPU Array map update (tiny) | tiny | 34.78 | 27.52 | 1.26x (+26.4%) |
| GPU Array map update (small_64x16) | small_64x16 | 36.95 | 28.54 | 1.29x (+29.5%) |
| GPU Array map update (small) | small | 36.39 | 28.54 | 1.28x (+27.5%) |
| GPU Array map update (medium) | medium | 54.78 | 27.80 | 1.97x (+97.1%) |
| GPU Array map update (large) | large | 229.15 | 31.59 | 7.25x (+625.4%) |
| GPU Array map update (xlarge) | xlarge | 1488.40 | 159.99 | 9.30x (+830.3%) |
| GPU Array map lookup (tiny) | tiny | 34.44 | 27.52 | 1.25x (+25.1%) |
| GPU Array map lookup (small_64x16) | small_64x16 | 34.35 | 28.54 | 1.20x (+20.4%) |
| GPU Array map lookup (small) | small | 38.13 | 28.54 | 1.34x (+33.6%) |
| GPU Array map lookup (medium) | medium | 36.36 | 27.80 | 1.31x (+30.8%) |
| GPU Array map lookup (large) | large | 68.65 | 31.59 | 2.17x (+117.3%) |
| GPU Array map lookup (xlarge) | xlarge | 440.12 | 159.99 | 2.75x (+175.1%) |
| CPU Array map update (minimal) | minimal | 318.79 | 27.52 | 11.58x (+1058.4%) |
| CPU Array map lookup (minimal) | minimal | 366.92 | 27.52 | 13.33x (+1233.3%) |
| CPU Hash map update (minimal) | minimal | 326.78 | 27.52 | 11.87x (+1087.4%) |
| CPU Hash map lookup (minimal) | minimal | 334.19 | 27.52 | 12.14x (+1114.4%) |
| CPU Hash map delete (minimal) | minimal | 319.43 | 27.52 | 11.61x (+1060.7%) |


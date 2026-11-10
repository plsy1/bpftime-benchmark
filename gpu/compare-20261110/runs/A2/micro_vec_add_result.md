
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T11:21:00.187416  

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
| Baseline (tiny) | tiny | 28.20 | - | - |
| Baseline (small) | small | 27.32 | - | - |
| Baseline (medium) | medium | 27.55 | - | - |
| Baseline (large) | large | 31.38 | - | - |
| Baseline (xlarge) | xlarge | 160.24 | - | - |
| Baseline (minimal) | minimal | 39.52 | - | - |
| Empty probe (tiny) | tiny | 33.38 | 28.20 | 1.18x (+18.4%) |
| Empty probe (small) | small | 32.33 | 27.32 | 1.18x (+18.3%) |
| Empty probe (medium) | medium | 33.96 | 27.55 | 1.23x (+23.3%) |
| Empty probe (large) | large | 46.52 | 31.38 | 1.48x (+48.2%) |
| Empty probe (xlarge) | xlarge | 175.22 | 160.24 | 1.09x (+9.3%) |
| Entry probe (tiny) | tiny | 29.73 | 28.20 | 1.05x (+5.4%) |
| Entry probe (small) | small | 33.90 | 27.32 | 1.24x (+24.1%) |
| Entry probe (medium) | medium | 35.92 | 27.55 | 1.30x (+30.4%) |
| Entry probe (xlarge) | xlarge | 179.06 | 160.24 | 1.12x (+11.7%) |
| Exit probe (tiny) | tiny | 29.46 | 28.20 | 1.04x (+4.5%) |
| Exit probe (small) | small | 35.18 | 27.32 | 1.29x (+28.8%) |
| Exit probe (large) | large | 45.18 | 31.38 | 1.44x (+44.0%) |
| Entry+Exit (tiny) | tiny | 34.48 | 28.20 | 1.22x (+22.3%) |
| Entry+Exit (small_64x16) | small_64x16 | 30.89 | 27.32 | 1.13x (+13.1%) |
| Entry+Exit (small) | small | 32.23 | 27.32 | 1.18x (+18.0%) |
| Entry+Exit (medium) | medium | 35.66 | 27.55 | 1.29x (+29.4%) |
| Entry+Exit (large) | large | 42.65 | 31.38 | 1.36x (+35.9%) |
| Entry+Exit (xlarge) | xlarge | 178.06 | 160.24 | 1.11x (+11.1%) |
| GPU Ringbuf (tiny) | tiny | 38.93 | 28.20 | 1.38x (+38.0%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 39.91 | 27.32 | 1.46x (+46.1%) |
| GPU Ringbuf (small) | small | 39.34 | 27.32 | 1.44x (+44.0%) |
| GPU Ringbuf (medium) | medium | 35.21 | 27.55 | 1.28x (+27.8%) |
| GPU Ringbuf (large) | large | 50.74 | 31.38 | 1.62x (+61.7%) |
| GPU Ringbuf (xlarge) | xlarge | 208.18 | 160.24 | 1.30x (+29.9%) |
| Global timer (tiny) | tiny | 32.87 | 28.20 | 1.17x (+16.6%) |
| Global timer (small) | small | 33.54 | 27.32 | 1.23x (+22.8%) |
| Global timer (medium) | medium | 31.50 | 27.55 | 1.14x (+14.3%) |
| Global timer (xlarge) | xlarge | 175.52 | 160.24 | 1.10x (+9.5%) |
| Per-GPU-thread array (tiny) | tiny | 39.86 | 28.20 | 1.41x (+41.3%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 40.05 | 27.32 | 1.47x (+46.6%) |
| Per-GPU-thread array (small) | small | 36.86 | 27.32 | 1.35x (+34.9%) |
| Memtrace (tiny) | tiny | 32.58 | 28.20 | 1.16x (+15.5%) |
| Memtrace (small) | small | 37.34 | 27.32 | 1.37x (+36.7%) |
| Memtrace (medium) | medium | 52.47 | 27.55 | 1.90x (+90.5%) |
| Memtrace (large) | large | 183.16 | 31.38 | 5.84x (+483.7%) |
| GPU Array map update (tiny) | tiny | 41.64 | 28.20 | 1.48x (+47.7%) |
| GPU Array map update (small_64x16) | small_64x16 | 41.23 | 27.32 | 1.51x (+50.9%) |
| GPU Array map update (small) | small | 41.12 | 27.32 | 1.51x (+50.5%) |
| GPU Array map update (medium) | medium | 49.47 | 27.55 | 1.80x (+79.6%) |
| GPU Array map update (large) | large | 220.31 | 31.38 | 7.02x (+602.1%) |
| GPU Array map update (xlarge) | xlarge | 1723.51 | 160.24 | 10.76x (+975.6%) |
| GPU Array map lookup (tiny) | tiny | 32.86 | 28.20 | 1.17x (+16.5%) |
| GPU Array map lookup (small_64x16) | small_64x16 | 32.54 | 27.32 | 1.19x (+19.1%) |
| GPU Array map lookup (small) | small | 34.73 | 27.32 | 1.27x (+27.1%) |
| GPU Array map lookup (medium) | medium | 38.18 | 27.55 | 1.39x (+38.6%) |
| GPU Array map lookup (large) | large | 67.77 | 31.38 | 2.16x (+116.0%) |
| GPU Array map lookup (xlarge) | xlarge | 454.87 | 160.24 | 2.84x (+183.9%) |
| CPU Array map update (minimal) | minimal | 33700.49 | 28.20 | 1195.05x (+119405.3%) |
| CPU Array map lookup (minimal) | minimal | 33744.73 | 28.20 | 1196.62x (+119562.2%) |
| CPU Hash map update (minimal) | minimal | 33725.66 | 28.20 | 1195.95x (+119494.5%) |
| CPU Hash map lookup (minimal) | minimal | 33752.42 | 28.20 | 1196.89x (+119589.4%) |
| CPU Hash map delete (minimal) | minimal | 33704.60 | 28.20 | 1195.20x (+119419.9%) |


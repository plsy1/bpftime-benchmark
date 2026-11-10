
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T11:33:34.293071  

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
| Baseline (tiny) | tiny | 27.65 | - | - |
| Baseline (small) | small | 26.55 | - | - |
| Baseline (medium) | medium | 25.89 | - | - |
| Baseline (large) | large | 31.23 | - | - |
| Baseline (xlarge) | xlarge | 159.69 | - | - |
| Baseline (minimal) | minimal | 39.04 | - | - |
| Empty probe (tiny) | tiny | 32.14 | 27.65 | 1.16x (+16.2%) |
| Empty probe (small) | small | 36.65 | 26.55 | 1.38x (+38.0%) |
| Empty probe (medium) | medium | 33.68 | 25.89 | 1.30x (+30.1%) |
| Empty probe (large) | large | 43.55 | 31.23 | 1.39x (+39.4%) |
| Empty probe (xlarge) | xlarge | 171.45 | 159.69 | 1.07x (+7.4%) |
| Entry probe (tiny) | tiny | 32.31 | 27.65 | 1.17x (+16.9%) |
| Entry probe (small) | small | 31.30 | 26.55 | 1.18x (+17.9%) |
| Entry probe (medium) | medium | 34.95 | 25.89 | 1.35x (+35.0%) |
| Entry probe (xlarge) | xlarge | 170.84 | 159.69 | 1.07x (+7.0%) |
| Exit probe (tiny) | tiny | 34.42 | 27.65 | 1.24x (+24.5%) |
| Exit probe (small) | small | 31.30 | 26.55 | 1.18x (+17.9%) |
| Exit probe (large) | large | 38.05 | 31.23 | 1.22x (+21.8%) |
| Entry+Exit (tiny) | tiny | 33.47 | 27.65 | 1.21x (+21.0%) |
| Entry+Exit (small_64x16) | small_64x16 | 36.45 | 26.55 | 1.37x (+37.3%) |
| Entry+Exit (small) | small | 35.18 | 26.55 | 1.33x (+32.5%) |
| Entry+Exit (medium) | medium | 37.96 | 25.89 | 1.47x (+46.6%) |
| Entry+Exit (large) | large | 39.38 | 31.23 | 1.26x (+26.1%) |
| Entry+Exit (xlarge) | xlarge | 177.61 | 159.69 | 1.11x (+11.2%) |
| GPU Ringbuf (tiny) | tiny | 35.15 | 27.65 | 1.27x (+27.1%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 42.98 | 26.55 | 1.62x (+61.9%) |
| GPU Ringbuf (small) | small | 49.47 | 26.55 | 1.86x (+86.3%) |
| GPU Ringbuf (medium) | medium | 43.49 | 25.89 | 1.68x (+68.0%) |
| GPU Ringbuf (large) | large | 56.69 | 31.23 | 1.82x (+81.5%) |
| GPU Ringbuf (xlarge) | xlarge | 212.78 | 159.69 | 1.33x (+33.2%) |
| Global timer (tiny) | tiny | 31.96 | 27.65 | 1.16x (+15.6%) |
| Global timer (small) | small | 36.74 | 26.55 | 1.38x (+38.4%) |
| Global timer (medium) | medium | 32.65 | 25.89 | 1.26x (+26.1%) |
| Global timer (xlarge) | xlarge | 176.86 | 159.69 | 1.11x (+10.8%) |
| Per-GPU-thread array (tiny) | tiny | 34.32 | 27.65 | 1.24x (+24.1%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 38.03 | 26.55 | 1.43x (+43.2%) |
| Per-GPU-thread array (small) | small | 38.98 | 26.55 | 1.47x (+46.8%) |
| Per-GPU-thread array (medium) | medium | 43.54 | 25.89 | 1.68x (+68.2%) |
| Per-GPU-thread array (large) | large | 143.23 | 31.23 | 4.59x (+358.6%) |
| Per-GPU-thread array (xlarge) | xlarge | 1162.13 | 159.69 | 7.28x (+627.7%) |
| Memtrace (tiny) | tiny | 36.71 | 27.65 | 1.33x (+32.8%) |
| Memtrace (small) | small | 36.64 | 26.55 | 1.38x (+38.0%) |
| Memtrace (medium) | medium | 47.76 | 25.89 | 1.84x (+84.5%) |
| Memtrace (large) | large | 170.55 | 31.23 | 5.46x (+446.1%) |
| GPU Array map update (tiny) | tiny | 38.27 | 27.65 | 1.38x (+38.4%) |
| GPU Array map update (small_64x16) | small_64x16 | 34.31 | 26.55 | 1.29x (+29.2%) |
| GPU Array map update (small) | small | 37.86 | 26.55 | 1.43x (+42.6%) |
| GPU Array map update (medium) | medium | 51.32 | 25.89 | 1.98x (+98.2%) |
| GPU Array map update (large) | large | 250.34 | 31.23 | 8.02x (+701.6%) |
| GPU Array map update (xlarge) | xlarge | 1485.72 | 159.69 | 9.30x (+830.4%) |
| GPU Array map lookup (tiny) | tiny | 32.37 | 27.65 | 1.17x (+17.1%) |
| GPU Array map lookup (small_64x16) | small_64x16 | 36.50 | 26.55 | 1.37x (+37.5%) |
| GPU Array map lookup (small) | small | 36.39 | 26.55 | 1.37x (+37.1%) |
| GPU Array map lookup (medium) | medium | 31.40 | 25.89 | 1.21x (+21.3%) |
| GPU Array map lookup (large) | large | 67.29 | 31.23 | 2.15x (+115.5%) |
| GPU Array map lookup (xlarge) | xlarge | 441.41 | 159.69 | 2.76x (+176.4%) |
| CPU Array map update (minimal) | minimal | 324.41 | 27.65 | 11.73x (+1073.3%) |
| CPU Array map lookup (minimal) | minimal | 363.63 | 27.65 | 13.15x (+1215.1%) |
| CPU Hash map update (minimal) | minimal | 318.06 | 27.65 | 11.50x (+1050.3%) |
| CPU Hash map lookup (minimal) | minimal | 336.75 | 27.65 | 12.18x (+1117.9%) |
| CPU Hash map delete (minimal) | minimal | 313.05 | 27.65 | 11.32x (+1032.2%) |



# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T11:56:55.637508  

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
| Baseline (tiny) | tiny | 27.09 | - | - |
| Baseline (small) | small | 28.13 | - | - |
| Baseline (medium) | medium | 27.50 | - | - |
| Baseline (large) | large | 31.03 | - | - |
| Baseline (xlarge) | xlarge | 159.39 | - | - |
| Baseline (minimal) | minimal | 39.70 | - | - |
| Empty probe (tiny) | tiny | 35.64 | 27.09 | 1.32x (+31.6%) |
| Empty probe (small) | small | 33.57 | 28.13 | 1.19x (+19.3%) |
| Empty probe (medium) | medium | 37.28 | 27.50 | 1.36x (+35.6%) |
| Empty probe (large) | large | 38.38 | 31.03 | 1.24x (+23.7%) |
| Empty probe (xlarge) | xlarge | 173.33 | 159.39 | 1.09x (+8.7%) |
| Entry probe (tiny) | tiny | 30.38 | 27.09 | 1.12x (+12.1%) |
| Entry probe (small) | small | 35.86 | 28.13 | 1.27x (+27.5%) |
| Entry probe (medium) | medium | 36.68 | 27.50 | 1.33x (+33.4%) |
| Entry probe (xlarge) | xlarge | 169.16 | 159.39 | 1.06x (+6.1%) |
| Exit probe (tiny) | tiny | 34.20 | 27.09 | 1.26x (+26.2%) |
| Exit probe (small) | small | 36.03 | 28.13 | 1.28x (+28.1%) |
| Exit probe (large) | large | 35.72 | 31.03 | 1.15x (+15.1%) |
| Entry+Exit (tiny) | tiny | 35.26 | 27.09 | 1.30x (+30.2%) |
| Entry+Exit (small_64x16) | small_64x16 | 33.00 | 28.13 | 1.17x (+17.3%) |
| Entry+Exit (small) | small | 34.84 | 28.13 | 1.24x (+23.9%) |
| Entry+Exit (medium) | medium | 34.02 | 27.50 | 1.24x (+23.7%) |
| Entry+Exit (large) | large | 38.79 | 31.03 | 1.25x (+25.0%) |
| Entry+Exit (xlarge) | xlarge | 170.96 | 159.39 | 1.07x (+7.3%) |
| GPU Ringbuf (tiny) | tiny | 31.93 | 27.09 | 1.18x (+17.9%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 46.62 | 28.13 | 1.66x (+65.7%) |
| GPU Ringbuf (small) | small | 46.84 | 28.13 | 1.67x (+66.5%) |
| GPU Ringbuf (medium) | medium | 40.38 | 27.50 | 1.47x (+46.8%) |
| GPU Ringbuf (large) | large | 51.87 | 31.03 | 1.67x (+67.2%) |
| GPU Ringbuf (xlarge) | xlarge | 219.89 | 159.39 | 1.38x (+38.0%) |
| Global timer (tiny) | tiny | 33.19 | 27.09 | 1.23x (+22.5%) |
| Global timer (small) | small | 33.90 | 28.13 | 1.21x (+20.5%) |
| Global timer (medium) | medium | 37.12 | 27.50 | 1.35x (+35.0%) |
| Global timer (xlarge) | xlarge | 175.54 | 159.39 | 1.10x (+10.1%) |
| Per-GPU-thread array (tiny) | tiny | 32.31 | 27.09 | 1.19x (+19.3%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 34.22 | 28.13 | 1.22x (+21.6%) |
| Per-GPU-thread array (small) | small | 39.32 | 28.13 | 1.40x (+39.8%) |
| Per-GPU-thread array (medium) | medium | 44.55 | 27.50 | 1.62x (+62.0%) |
| Per-GPU-thread array (large) | large | 148.06 | 31.03 | 4.77x (+377.2%) |
| Per-GPU-thread array (xlarge) | xlarge | 1165.00 | 159.39 | 7.31x (+630.9%) |
| Memtrace (tiny) | tiny | 36.60 | 27.09 | 1.35x (+35.1%) |
| Memtrace (small) | small | 38.95 | 28.13 | 1.38x (+38.5%) |
| Memtrace (medium) | medium | 47.52 | 27.50 | 1.73x (+72.8%) |
| Memtrace (large) | large | 171.16 | 31.03 | 5.52x (+451.6%) |
| GPU Array map update (tiny) | tiny | 39.96 | 27.09 | 1.48x (+47.5%) |
| GPU Array map update (small_64x16) | small_64x16 | 33.04 | 28.13 | 1.17x (+17.5%) |
| GPU Array map update (small) | small | 34.12 | 28.13 | 1.21x (+21.3%) |
| GPU Array map update (medium) | medium | 47.95 | 27.50 | 1.74x (+74.4%) |
| GPU Array map update (large) | large | 249.56 | 31.03 | 8.04x (+704.3%) |
| GPU Array map update (xlarge) | xlarge | 1394.91 | 159.39 | 8.75x (+775.2%) |
| GPU Array map lookup (tiny) | tiny | 38.79 | 27.09 | 1.43x (+43.2%) |
| GPU Array map lookup (small_64x16) | small_64x16 | 39.58 | 28.13 | 1.41x (+40.7%) |
| GPU Array map lookup (small) | small | 33.30 | 28.13 | 1.18x (+18.4%) |
| GPU Array map lookup (medium) | medium | 37.46 | 27.50 | 1.36x (+36.2%) |
| GPU Array map lookup (large) | large | 71.68 | 31.03 | 2.31x (+131.0%) |
| GPU Array map lookup (xlarge) | xlarge | 443.70 | 159.39 | 2.78x (+178.4%) |
| CPU Array map update (minimal) | minimal | 326.68 | 27.09 | 12.06x (+1105.9%) |
| CPU Array map lookup (minimal) | minimal | 375.03 | 27.09 | 13.84x (+1284.4%) |
| CPU Hash map update (minimal) | minimal | 322.76 | 27.09 | 11.91x (+1091.4%) |
| CPU Hash map lookup (minimal) | minimal | 344.73 | 27.09 | 12.73x (+1172.5%) |
| CPU Hash map delete (minimal) | minimal | 321.07 | 27.09 | 11.85x (+1085.2%) |


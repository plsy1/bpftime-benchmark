
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T11:44:21.204780  

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
| Baseline (tiny) | tiny | 27.79 | - | - |
| Baseline (small) | small | 27.79 | - | - |
| Baseline (medium) | medium | 27.57 | - | - |
| Baseline (large) | large | 31.21 | - | - |
| Baseline (xlarge) | xlarge | 158.86 | - | - |
| Baseline (minimal) | minimal | 37.89 | - | - |
| Empty probe (tiny) | tiny | 33.95 | 27.79 | 1.22x (+22.2%) |
| Empty probe (small) | small | 31.36 | 27.79 | 1.13x (+12.8%) |
| Empty probe (medium) | medium | 36.69 | 27.57 | 1.33x (+33.1%) |
| Empty probe (large) | large | 44.57 | 31.21 | 1.43x (+42.8%) |
| Empty probe (xlarge) | xlarge | 173.45 | 158.86 | 1.09x (+9.2%) |
| Entry probe (tiny) | tiny | 32.24 | 27.79 | 1.16x (+16.0%) |
| Entry probe (small) | small | 29.88 | 27.79 | 1.08x (+7.5%) |
| Entry probe (medium) | medium | 31.75 | 27.57 | 1.15x (+15.2%) |
| Entry probe (xlarge) | xlarge | 173.24 | 158.86 | 1.09x (+9.1%) |
| Exit probe (tiny) | tiny | 28.75 | 27.79 | 1.03x (+3.5%) |
| Exit probe (small) | small | 33.13 | 27.79 | 1.19x (+19.2%) |
| Exit probe (large) | large | 47.53 | 31.21 | 1.52x (+52.3%) |
| Entry+Exit (tiny) | tiny | 31.11 | 27.79 | 1.12x (+11.9%) |
| Entry+Exit (small_64x16) | small_64x16 | 31.85 | 27.79 | 1.15x (+14.6%) |
| Entry+Exit (small) | small | 30.34 | 27.79 | 1.09x (+9.2%) |
| Entry+Exit (medium) | medium | 35.24 | 27.57 | 1.28x (+27.8%) |
| Entry+Exit (large) | large | 45.64 | 31.21 | 1.46x (+46.2%) |
| Entry+Exit (xlarge) | xlarge | 176.25 | 158.86 | 1.11x (+10.9%) |
| GPU Ringbuf (tiny) | tiny | 37.55 | 27.79 | 1.35x (+35.1%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 39.99 | 27.79 | 1.44x (+43.9%) |
| GPU Ringbuf (small) | small | 41.20 | 27.79 | 1.48x (+48.3%) |
| GPU Ringbuf (medium) | medium | 39.22 | 27.57 | 1.42x (+42.3%) |
| GPU Ringbuf (large) | large | 51.71 | 31.21 | 1.66x (+65.7%) |
| GPU Ringbuf (xlarge) | xlarge | 209.69 | 158.86 | 1.32x (+32.0%) |
| Global timer (tiny) | tiny | 31.27 | 27.79 | 1.13x (+12.5%) |
| Global timer (small) | small | 34.97 | 27.79 | 1.26x (+25.8%) |
| Global timer (medium) | medium | 37.39 | 27.57 | 1.36x (+35.6%) |
| Global timer (xlarge) | xlarge | 181.30 | 158.86 | 1.14x (+14.1%) |
| Per-GPU-thread array (tiny) | tiny | 38.24 | 27.79 | 1.38x (+37.6%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 39.32 | 27.79 | 1.41x (+41.5%) |
| Per-GPU-thread array (small) | small | 39.61 | 27.79 | 1.43x (+42.5%) |
| Memtrace (tiny) | tiny | 35.27 | 27.79 | 1.27x (+26.9%) |
| Memtrace (small) | small | 39.75 | 27.79 | 1.43x (+43.0%) |
| Memtrace (medium) | medium | 46.77 | 27.57 | 1.70x (+69.6%) |
| Memtrace (large) | large | 179.89 | 31.21 | 5.76x (+476.4%) |
| GPU Array map update (tiny) | tiny | 38.69 | 27.79 | 1.39x (+39.2%) |
| GPU Array map update (small_64x16) | small_64x16 | 41.16 | 27.79 | 1.48x (+48.1%) |
| GPU Array map update (small) | small | 40.46 | 27.79 | 1.46x (+45.6%) |
| GPU Array map update (medium) | medium | 48.79 | 27.57 | 1.77x (+77.0%) |
| GPU Array map update (large) | large | 225.50 | 31.21 | 7.23x (+622.5%) |
| GPU Array map update (xlarge) | xlarge | 1779.30 | 158.86 | 11.20x (+1020.0%) |
| GPU Array map lookup (tiny) | tiny | 32.60 | 27.79 | 1.17x (+17.3%) |
| GPU Array map lookup (small_64x16) | small_64x16 | 37.92 | 27.79 | 1.36x (+36.5%) |
| GPU Array map lookup (small) | small | 38.05 | 27.79 | 1.37x (+36.9%) |
| GPU Array map lookup (medium) | medium | 39.33 | 27.57 | 1.43x (+42.7%) |
| GPU Array map lookup (large) | large | 71.09 | 31.21 | 2.28x (+127.8%) |
| GPU Array map lookup (xlarge) | xlarge | 455.65 | 158.86 | 2.87x (+186.8%) |
| CPU Array map update (minimal) | minimal | 33727.04 | 27.79 | 1213.64x (+121263.9%) |
| CPU Array map lookup (minimal) | minimal | 33793.77 | 27.79 | 1216.04x (+121504.1%) |
| CPU Hash map update (minimal) | minimal | 33671.50 | 27.79 | 1211.64x (+121064.1%) |
| CPU Hash map lookup (minimal) | minimal | 33708.18 | 27.79 | 1212.96x (+121196.1%) |
| CPU Hash map delete (minimal) | minimal | 33664.98 | 27.79 | 1211.41x (+121040.6%) |


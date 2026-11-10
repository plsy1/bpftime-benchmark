
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T10:57:40.661683  

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
| Baseline (small) | small | 27.70 | - | - |
| Baseline (medium) | medium | 26.51 | - | - |
| Baseline (large) | large | 32.34 | - | - |
| Baseline (xlarge) | xlarge | 159.76 | - | - |
| Baseline (minimal) | minimal | 39.61 | - | - |
| Empty probe (tiny) | tiny | 30.47 | 27.52 | 1.11x (+10.7%) |
| Empty probe (small) | small | 35.51 | 27.70 | 1.28x (+28.2%) |
| Empty probe (medium) | medium | 33.94 | 26.51 | 1.28x (+28.0%) |
| Empty probe (large) | large | 42.33 | 32.34 | 1.31x (+30.9%) |
| Empty probe (xlarge) | xlarge | 176.32 | 159.76 | 1.10x (+10.4%) |
| Entry probe (tiny) | tiny | 30.91 | 27.52 | 1.12x (+12.3%) |
| Entry probe (small) | small | 34.03 | 27.70 | 1.23x (+22.9%) |
| Entry probe (medium) | medium | 36.54 | 26.51 | 1.38x (+37.8%) |
| Entry probe (xlarge) | xlarge | 173.14 | 159.76 | 1.08x (+8.4%) |
| Exit probe (tiny) | tiny | 35.01 | 27.52 | 1.27x (+27.2%) |
| Exit probe (small) | small | 32.24 | 27.70 | 1.16x (+16.4%) |
| Exit probe (large) | large | 39.98 | 32.34 | 1.24x (+23.6%) |
| Entry+Exit (tiny) | tiny | 34.02 | 27.52 | 1.24x (+23.6%) |
| Entry+Exit (small_64x16) | small_64x16 | 30.54 | 27.70 | 1.10x (+10.3%) |
| Entry+Exit (small) | small | 34.60 | 27.70 | 1.25x (+24.9%) |
| Entry+Exit (medium) | medium | 36.96 | 26.51 | 1.39x (+39.4%) |
| Entry+Exit (large) | large | 47.67 | 32.34 | 1.47x (+47.4%) |
| Entry+Exit (xlarge) | xlarge | 173.29 | 159.76 | 1.08x (+8.5%) |
| GPU Ringbuf (tiny) | tiny | 33.38 | 27.52 | 1.21x (+21.3%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 41.03 | 27.70 | 1.48x (+48.1%) |
| GPU Ringbuf (small) | small | 37.54 | 27.70 | 1.36x (+35.5%) |
| GPU Ringbuf (medium) | medium | 39.01 | 26.51 | 1.47x (+47.2%) |
| GPU Ringbuf (large) | large | 49.06 | 32.34 | 1.52x (+51.7%) |
| GPU Ringbuf (xlarge) | xlarge | 212.49 | 159.76 | 1.33x (+33.0%) |
| Global timer (tiny) | tiny | 30.77 | 27.52 | 1.12x (+11.8%) |
| Global timer (small) | small | 31.42 | 27.70 | 1.13x (+13.4%) |
| Global timer (medium) | medium | 36.44 | 26.51 | 1.37x (+37.5%) |
| Global timer (xlarge) | xlarge | 174.16 | 159.76 | 1.09x (+9.0%) |
| Per-GPU-thread array (tiny) | tiny | 36.56 | 27.52 | 1.33x (+32.8%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 39.22 | 27.70 | 1.42x (+41.6%) |
| Per-GPU-thread array (small) | small | 40.76 | 27.70 | 1.47x (+47.1%) |
| Memtrace (tiny) | tiny | 34.63 | 27.52 | 1.26x (+25.8%) |
| Memtrace (small) | small | 36.61 | 27.70 | 1.32x (+32.2%) |
| Memtrace (medium) | medium | 49.98 | 26.51 | 1.89x (+88.5%) |
| Memtrace (large) | large | 181.06 | 32.34 | 5.60x (+459.9%) |
| GPU Array map update (tiny) | tiny | 38.79 | 27.52 | 1.41x (+41.0%) |
| GPU Array map update (small_64x16) | small_64x16 | 40.67 | 27.70 | 1.47x (+46.8%) |
| GPU Array map update (small) | small | 40.62 | 27.70 | 1.47x (+46.6%) |
| GPU Array map update (medium) | medium | 48.56 | 26.51 | 1.83x (+83.2%) |
| GPU Array map update (large) | large | 220.06 | 32.34 | 6.80x (+580.5%) |
| GPU Array map update (xlarge) | xlarge | 1721.67 | 159.76 | 10.78x (+977.7%) |
| GPU Array map lookup (tiny) | tiny | 32.60 | 27.52 | 1.18x (+18.5%) |
| GPU Array map lookup (small_64x16) | small_64x16 | 36.99 | 27.70 | 1.34x (+33.5%) |
| GPU Array map lookup (small) | small | 34.55 | 27.70 | 1.25x (+24.7%) |
| GPU Array map lookup (medium) | medium | 39.47 | 26.51 | 1.49x (+48.9%) |
| GPU Array map lookup (large) | large | 68.63 | 32.34 | 2.12x (+112.2%) |
| GPU Array map lookup (xlarge) | xlarge | 458.79 | 159.76 | 2.87x (+187.2%) |
| CPU Array map update (minimal) | minimal | 33713.00 | 27.52 | 1225.04x (+122403.6%) |
| CPU Array map lookup (minimal) | minimal | 33734.35 | 27.52 | 1225.81x (+122481.2%) |
| CPU Hash map update (minimal) | minimal | 33666.50 | 27.52 | 1223.35x (+122234.7%) |
| CPU Hash map lookup (minimal) | minimal | 33691.12 | 27.52 | 1224.24x (+122324.1%) |
| CPU Hash map delete (minimal) | minimal | 33691.92 | 27.52 | 1224.27x (+122327.0%) |


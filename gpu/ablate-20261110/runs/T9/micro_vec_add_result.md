
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T18:23:16.865090  

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
| Baseline (tiny) | tiny | 27.27 | - | - |
| Baseline (small) | small | 27.55 | - | - |
| Baseline (medium) | medium | 27.76 | - | - |
| Baseline (large) | large | 31.33 | - | - |
| Baseline (xlarge) | xlarge | 159.26 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 28.89 | 27.55 | 1.05x (+4.9%) |
| Entry+Exit (small) | small | 34.65 | 27.55 | 1.26x (+25.8%) |
| Entry+Exit (large) | large | 40.22 | 31.33 | 1.28x (+28.4%) |
| Empty probe (large) | large | 39.29 | 31.33 | 1.25x (+25.4%) |
| Empty probe (xlarge) | xlarge | 169.78 | 159.26 | 1.07x (+6.6%) |
| Entry probe (xlarge) | xlarge | 167.52 | 159.26 | 1.05x (+5.2%) |
| Memtrace (tiny) | tiny | 34.66 | 27.27 | 1.27x (+27.1%) |
| Memtrace (large) | large | 175.53 | 31.33 | 5.60x (+460.3%) |
| GPU Array map lookup (xlarge) | xlarge | 443.04 | 159.26 | 2.78x (+178.2%) |
| Per-GPU-thread array (tiny) | tiny | 35.59 | 27.27 | 1.31x (+30.5%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 35.50 | 27.55 | 1.29x (+28.9%) |
| GPU Array map update (small_64x16) | small_64x16 | 35.40 | 27.55 | 1.28x (+28.5%) |
| GPU Array map update (small) | small | 39.04 | 27.55 | 1.42x (+41.7%) |
| GPU Array map update (large) | large | 231.72 | 31.33 | 7.40x (+639.6%) |
| GPU Array map update (xlarge) | xlarge | 1473.19 | 159.26 | 9.25x (+825.0%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 34.68 | 27.55 | 1.26x (+25.9%) |
| GPU Ringbuf (small) | small | 38.71 | 27.55 | 1.41x (+40.5%) |
| GPU Ringbuf (medium) | medium | 31.80 | 27.76 | 1.15x (+14.6%) |


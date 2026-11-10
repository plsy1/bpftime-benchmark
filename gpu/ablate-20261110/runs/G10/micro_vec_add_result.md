
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T18:38:19.214811  

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
| Baseline (tiny) | tiny | 27.21 | - | - |
| Baseline (small) | small | 28.22 | - | - |
| Baseline (medium) | medium | 27.39 | - | - |
| Baseline (large) | large | 31.34 | - | - |
| Baseline (xlarge) | xlarge | 159.21 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 33.45 | 28.22 | 1.19x (+18.5%) |
| Entry+Exit (small) | small | 30.90 | 28.22 | 1.09x (+9.5%) |
| Entry+Exit (large) | large | 42.56 | 31.34 | 1.36x (+35.8%) |
| Empty probe (large) | large | 35.39 | 31.34 | 1.13x (+12.9%) |
| Empty probe (xlarge) | xlarge | 176.11 | 159.21 | 1.11x (+10.6%) |
| Entry probe (xlarge) | xlarge | 169.87 | 159.21 | 1.07x (+6.7%) |
| Memtrace (tiny) | tiny | 33.97 | 27.21 | 1.25x (+24.8%) |
| Memtrace (large) | large | 172.00 | 31.34 | 5.49x (+448.8%) |
| GPU Array map lookup (xlarge) | xlarge | 459.37 | 159.21 | 2.89x (+188.5%) |
| Per-GPU-thread array (tiny) | tiny | 35.31 | 27.21 | 1.30x (+29.8%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 38.09 | 28.22 | 1.35x (+35.0%) |
| GPU Array map update (small_64x16) | small_64x16 | 37.57 | 28.22 | 1.33x (+33.1%) |
| GPU Array map update (small) | small | 35.49 | 28.22 | 1.26x (+25.8%) |
| GPU Array map update (large) | large | 249.05 | 31.34 | 7.95x (+694.7%) |
| GPU Array map update (xlarge) | xlarge | 1498.33 | 159.21 | 9.41x (+841.1%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 45.99 | 28.22 | 1.63x (+63.0%) |
| GPU Ringbuf (small) | small | 39.06 | 28.22 | 1.38x (+38.4%) |
| GPU Ringbuf (medium) | medium | 44.19 | 27.39 | 1.61x (+61.3%) |



# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T17:28:13.973708  

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
| Baseline (tiny) | tiny | 27.33 | - | - |
| Baseline (small) | small | 27.44 | - | - |
| Baseline (medium) | medium | 26.23 | - | - |
| Baseline (large) | large | 31.25 | - | - |
| Baseline (xlarge) | xlarge | 159.56 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 35.35 | 27.44 | 1.29x (+28.8%) |
| Entry+Exit (small) | small | 34.61 | 27.44 | 1.26x (+26.1%) |
| Entry+Exit (large) | large | 45.73 | 31.25 | 1.46x (+46.3%) |
| Empty probe (large) | large | 40.17 | 31.25 | 1.29x (+28.5%) |
| Empty probe (xlarge) | xlarge | 175.29 | 159.56 | 1.10x (+9.9%) |
| Entry probe (xlarge) | xlarge | 172.90 | 159.56 | 1.08x (+8.4%) |
| Memtrace (tiny) | tiny | 31.04 | 27.33 | 1.14x (+13.6%) |
| Memtrace (large) | large | 177.55 | 31.25 | 5.68x (+468.2%) |
| GPU Array map lookup (xlarge) | xlarge | 459.49 | 159.56 | 2.88x (+188.0%) |
| Per-GPU-thread array (tiny) | tiny | 37.48 | 27.33 | 1.37x (+37.1%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 38.85 | 27.44 | 1.42x (+41.6%) |
| GPU Array map update (small_64x16) | small_64x16 | 39.99 | 27.44 | 1.46x (+45.7%) |
| GPU Array map update (small) | small | 39.92 | 27.44 | 1.45x (+45.5%) |
| GPU Array map update (large) | large | 227.62 | 31.25 | 7.28x (+628.4%) |
| GPU Array map update (xlarge) | xlarge | 1649.07 | 159.56 | 10.34x (+933.5%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 39.80 | 27.44 | 1.45x (+45.0%) |
| GPU Ringbuf (small) | small | 39.24 | 27.44 | 1.43x (+43.0%) |
| GPU Ringbuf (medium) | medium | 39.01 | 26.23 | 1.49x (+48.7%) |


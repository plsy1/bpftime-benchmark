
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T15:30:10.758031  

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
| Baseline (tiny) | tiny | 27.82 | - | - |
| Baseline (small) | small | 27.22 | - | - |
| Baseline (medium) | medium | 27.28 | - | - |
| Baseline (large) | large | 31.44 | - | - |
| Baseline (xlarge) | xlarge | 160.01 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 35.85 | 27.22 | 1.32x (+31.7%) |
| Entry+Exit (small) | small | 29.91 | 27.22 | 1.10x (+9.9%) |
| Entry+Exit (large) | large | 36.96 | 31.44 | 1.18x (+17.6%) |
| Empty probe (large) | large | 36.28 | 31.44 | 1.15x (+15.4%) |
| Empty probe (xlarge) | xlarge | 171.66 | 160.01 | 1.07x (+7.3%) |
| Entry probe (xlarge) | xlarge | 172.45 | 160.01 | 1.08x (+7.8%) |
| Memtrace (tiny) | tiny | 31.60 | 27.82 | 1.14x (+13.6%) |
| Memtrace (large) | large | 176.67 | 31.44 | 5.62x (+461.9%) |
| GPU Array map lookup (xlarge) | xlarge | 461.01 | 160.01 | 2.88x (+188.1%) |
| Per-GPU-thread array (tiny) | tiny | 32.94 | 27.82 | 1.18x (+18.4%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 35.39 | 27.22 | 1.30x (+30.0%) |
| GPU Array map update (small_64x16) | small_64x16 | 37.36 | 27.22 | 1.37x (+37.3%) |
| GPU Array map update (small) | small | 39.60 | 27.22 | 1.45x (+45.5%) |
| GPU Array map update (large) | large | 225.73 | 31.44 | 7.18x (+618.0%) |
| GPU Array map update (xlarge) | xlarge | 1541.23 | 160.01 | 9.63x (+863.2%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 46.13 | 27.22 | 1.69x (+69.5%) |
| GPU Ringbuf (small) | small | 45.43 | 27.22 | 1.67x (+66.9%) |
| GPU Ringbuf (medium) | medium | 43.43 | 27.28 | 1.59x (+59.2%) |


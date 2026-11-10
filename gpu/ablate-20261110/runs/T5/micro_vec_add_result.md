
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T16:47:25.352591  

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
| Baseline (tiny) | tiny | 26.19 | - | - |
| Baseline (small) | small | 27.26 | - | - |
| Baseline (medium) | medium | 27.07 | - | - |
| Baseline (large) | large | 31.31 | - | - |
| Baseline (xlarge) | xlarge | 159.20 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 33.19 | 27.26 | 1.22x (+21.8%) |
| Entry+Exit (small) | small | 30.17 | 27.26 | 1.11x (+10.7%) |
| Entry+Exit (large) | large | 37.03 | 31.31 | 1.18x (+18.3%) |
| Empty probe (large) | large | 44.44 | 31.31 | 1.42x (+41.9%) |
| Empty probe (xlarge) | xlarge | 170.54 | 159.20 | 1.07x (+7.1%) |
| Entry probe (xlarge) | xlarge | 166.97 | 159.20 | 1.05x (+4.9%) |
| Memtrace (tiny) | tiny | 34.59 | 26.19 | 1.32x (+32.1%) |
| Memtrace (large) | large | 173.87 | 31.31 | 5.55x (+455.3%) |
| GPU Array map lookup (xlarge) | xlarge | 442.15 | 159.20 | 2.78x (+177.7%) |
| Per-GPU-thread array (tiny) | tiny | 36.96 | 26.19 | 1.41x (+41.1%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 32.36 | 27.26 | 1.19x (+18.7%) |
| GPU Array map update (small_64x16) | small_64x16 | 34.35 | 27.26 | 1.26x (+26.0%) |
| GPU Array map update (small) | small | 37.88 | 27.26 | 1.39x (+39.0%) |
| GPU Array map update (large) | large | 235.70 | 31.31 | 7.53x (+652.8%) |
| GPU Array map update (xlarge) | xlarge | 1420.13 | 159.20 | 8.92x (+792.0%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 38.27 | 27.26 | 1.40x (+40.4%) |
| GPU Ringbuf (small) | small | 37.19 | 27.26 | 1.36x (+36.4%) |
| GPU Ringbuf (medium) | medium | 37.62 | 27.07 | 1.39x (+39.0%) |


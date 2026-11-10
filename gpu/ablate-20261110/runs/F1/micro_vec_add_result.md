
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T15:26:37.246725  

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
| Baseline (tiny) | tiny | 26.88 | - | - |
| Baseline (small) | small | 27.78 | - | - |
| Baseline (medium) | medium | 27.57 | - | - |
| Baseline (large) | large | 31.19 | - | - |
| Baseline (xlarge) | xlarge | 159.73 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 31.60 | 27.78 | 1.14x (+13.8%) |
| Entry+Exit (small) | small | 35.95 | 27.78 | 1.29x (+29.4%) |
| Entry+Exit (large) | large | 36.89 | 31.19 | 1.18x (+18.3%) |
| Empty probe (large) | large | 41.30 | 31.19 | 1.32x (+32.4%) |
| Empty probe (xlarge) | xlarge | 172.81 | 159.73 | 1.08x (+8.2%) |
| Entry probe (xlarge) | xlarge | 170.86 | 159.73 | 1.07x (+7.0%) |
| Memtrace (tiny) | tiny | 32.95 | 26.88 | 1.23x (+22.6%) |
| Memtrace (large) | large | 175.55 | 31.19 | 5.63x (+462.8%) |
| GPU Array map lookup (xlarge) | xlarge | 460.61 | 159.73 | 2.88x (+188.4%) |
| Per-GPU-thread array (tiny) | tiny | 34.02 | 26.88 | 1.27x (+26.6%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 36.09 | 27.78 | 1.30x (+29.9%) |
| GPU Array map update (small_64x16) | small_64x16 | 34.95 | 27.78 | 1.26x (+25.8%) |
| GPU Array map update (small) | small | 34.31 | 27.78 | 1.24x (+23.5%) |
| GPU Array map update (large) | large | 243.69 | 31.19 | 7.81x (+681.3%) |
| GPU Array map update (xlarge) | xlarge | 1501.08 | 159.73 | 9.40x (+839.8%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 44.16 | 27.78 | 1.59x (+59.0%) |
| GPU Ringbuf (small) | small | 46.82 | 27.78 | 1.69x (+68.5%) |
| GPU Ringbuf (medium) | medium | 41.93 | 27.57 | 1.52x (+52.1%) |


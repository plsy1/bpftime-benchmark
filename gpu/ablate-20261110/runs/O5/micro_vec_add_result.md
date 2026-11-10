
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T16:50:58.735604  

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
| Baseline (tiny) | tiny | 27.04 | - | - |
| Baseline (small) | small | 27.28 | - | - |
| Baseline (medium) | medium | 27.41 | - | - |
| Baseline (large) | large | 31.65 | - | - |
| Baseline (xlarge) | xlarge | 159.59 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 35.09 | 27.28 | 1.29x (+28.6%) |
| Entry+Exit (small) | small | 30.85 | 27.28 | 1.13x (+13.1%) |
| Entry+Exit (large) | large | 45.22 | 31.65 | 1.43x (+42.9%) |
| Empty probe (large) | large | 44.58 | 31.65 | 1.41x (+40.9%) |
| Empty probe (xlarge) | xlarge | 174.38 | 159.59 | 1.09x (+9.3%) |
| Entry probe (xlarge) | xlarge | 174.10 | 159.59 | 1.09x (+9.1%) |
| Memtrace (tiny) | tiny | 32.10 | 27.04 | 1.19x (+18.7%) |
| Memtrace (large) | large | 180.03 | 31.65 | 5.69x (+468.8%) |
| GPU Array map lookup (xlarge) | xlarge | 483.98 | 159.59 | 3.03x (+203.3%) |
| Per-GPU-thread array (tiny) | tiny | 39.44 | 27.04 | 1.46x (+45.9%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 36.86 | 27.28 | 1.35x (+35.1%) |
| GPU Array map update (small_64x16) | small_64x16 | 39.33 | 27.28 | 1.44x (+44.2%) |
| GPU Array map update (small) | small | 39.40 | 27.28 | 1.44x (+44.4%) |
| GPU Array map update (large) | large | 223.23 | 31.65 | 7.05x (+605.3%) |
| GPU Array map update (xlarge) | xlarge | 1643.74 | 159.59 | 10.30x (+930.0%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 36.89 | 27.28 | 1.35x (+35.2%) |
| GPU Ringbuf (small) | small | 33.98 | 27.28 | 1.25x (+24.6%) |
| GPU Ringbuf (medium) | medium | 38.92 | 27.41 | 1.42x (+42.0%) |


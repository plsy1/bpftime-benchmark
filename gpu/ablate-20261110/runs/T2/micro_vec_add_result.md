
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T15:51:33.427774  

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
| Baseline (tiny) | tiny | 27.56 | - | - |
| Baseline (small) | small | 26.71 | - | - |
| Baseline (medium) | medium | 27.53 | - | - |
| Baseline (large) | large | 31.26 | - | - |
| Baseline (xlarge) | xlarge | 160.45 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 37.02 | 26.71 | 1.39x (+38.6%) |
| Entry+Exit (small) | small | 35.96 | 26.71 | 1.35x (+34.6%) |
| Entry+Exit (large) | large | 41.85 | 31.26 | 1.34x (+33.9%) |
| Empty probe (large) | large | 36.37 | 31.26 | 1.16x (+16.3%) |
| Empty probe (xlarge) | xlarge | 173.83 | 160.45 | 1.08x (+8.3%) |
| Entry probe (xlarge) | xlarge | 171.56 | 160.45 | 1.07x (+6.9%) |
| Memtrace (tiny) | tiny | 34.39 | 27.56 | 1.25x (+24.8%) |
| Memtrace (large) | large | 176.30 | 31.26 | 5.64x (+464.0%) |
| GPU Array map lookup (xlarge) | xlarge | 437.94 | 160.45 | 2.73x (+172.9%) |
| Per-GPU-thread array (tiny) | tiny | 33.38 | 27.56 | 1.21x (+21.1%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 34.79 | 26.71 | 1.30x (+30.3%) |
| GPU Array map update (small_64x16) | small_64x16 | 35.98 | 26.71 | 1.35x (+34.7%) |
| GPU Array map update (small) | small | 34.38 | 26.71 | 1.29x (+28.7%) |
| GPU Array map update (large) | large | 233.22 | 31.26 | 7.46x (+646.1%) |
| GPU Array map update (xlarge) | xlarge | 1452.07 | 160.45 | 9.05x (+805.0%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 33.08 | 26.71 | 1.24x (+23.8%) |
| GPU Ringbuf (small) | small | 38.95 | 26.71 | 1.46x (+45.8%) |
| GPU Ringbuf (medium) | medium | 36.32 | 27.53 | 1.32x (+31.9%) |


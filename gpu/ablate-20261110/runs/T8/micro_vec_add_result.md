
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T18:04:39.134713  

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
| Baseline (tiny) | tiny | 28.20 | - | - |
| Baseline (small) | small | 27.48 | - | - |
| Baseline (medium) | medium | 26.92 | - | - |
| Baseline (large) | large | 34.35 | - | - |
| Baseline (xlarge) | xlarge | 160.78 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 32.51 | 27.48 | 1.18x (+18.3%) |
| Entry+Exit (small) | small | 35.21 | 27.48 | 1.28x (+28.1%) |
| Entry+Exit (large) | large | 42.13 | 34.35 | 1.23x (+22.6%) |
| Empty probe (large) | large | 35.67 | 34.35 | 1.04x (+3.8%) |
| Empty probe (xlarge) | xlarge | 172.79 | 160.78 | 1.07x (+7.5%) |
| Entry probe (xlarge) | xlarge | 165.79 | 160.78 | 1.03x (+3.1%) |
| Memtrace (tiny) | tiny | 32.41 | 28.20 | 1.15x (+14.9%) |
| Memtrace (large) | large | 170.39 | 34.35 | 4.96x (+396.0%) |
| GPU Array map lookup (xlarge) | xlarge | 446.99 | 160.78 | 2.78x (+178.0%) |
| Per-GPU-thread array (tiny) | tiny | 34.60 | 28.20 | 1.23x (+22.7%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 38.89 | 27.48 | 1.42x (+41.5%) |
| GPU Array map update (small_64x16) | small_64x16 | 34.63 | 27.48 | 1.26x (+26.0%) |
| GPU Array map update (small) | small | 39.23 | 27.48 | 1.43x (+42.8%) |
| GPU Array map update (large) | large | 228.60 | 34.35 | 6.66x (+565.5%) |
| GPU Array map update (xlarge) | xlarge | 1492.01 | 160.78 | 9.28x (+828.0%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 36.80 | 27.48 | 1.34x (+33.9%) |
| GPU Ringbuf (small) | small | 37.19 | 27.48 | 1.35x (+35.3%) |
| GPU Ringbuf (medium) | medium | 30.87 | 26.92 | 1.15x (+14.7%) |


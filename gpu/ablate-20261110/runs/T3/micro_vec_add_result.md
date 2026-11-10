
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T16:10:12.115418  

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
| Baseline (tiny) | tiny | 28.45 | - | - |
| Baseline (small) | small | 26.28 | - | - |
| Baseline (medium) | medium | 27.46 | - | - |
| Baseline (large) | large | 30.48 | - | - |
| Baseline (xlarge) | xlarge | 159.04 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 33.92 | 26.28 | 1.29x (+29.1%) |
| Entry+Exit (small) | small | 35.59 | 26.28 | 1.35x (+35.4%) |
| Entry+Exit (large) | large | 37.98 | 30.48 | 1.25x (+24.6%) |
| Empty probe (large) | large | 40.60 | 30.48 | 1.33x (+33.2%) |
| Empty probe (xlarge) | xlarge | 170.43 | 159.04 | 1.07x (+7.2%) |
| Entry probe (xlarge) | xlarge | 173.89 | 159.04 | 1.09x (+9.3%) |
| Memtrace (tiny) | tiny | 34.77 | 28.45 | 1.22x (+22.2%) |
| Memtrace (large) | large | 175.78 | 30.48 | 5.77x (+476.7%) |
| GPU Array map lookup (xlarge) | xlarge | 446.39 | 159.04 | 2.81x (+180.7%) |
| Per-GPU-thread array (tiny) | tiny | 37.64 | 28.45 | 1.32x (+32.3%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 33.52 | 26.28 | 1.28x (+27.5%) |
| GPU Array map update (small_64x16) | small_64x16 | 33.72 | 26.28 | 1.28x (+28.3%) |
| GPU Array map update (small) | small | 34.02 | 26.28 | 1.29x (+29.5%) |
| GPU Array map update (large) | large | 234.81 | 30.48 | 7.70x (+670.4%) |
| GPU Array map update (xlarge) | xlarge | 1621.68 | 159.04 | 10.20x (+919.7%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 31.38 | 26.28 | 1.19x (+19.4%) |
| GPU Ringbuf (small) | small | 34.44 | 26.28 | 1.31x (+31.1%) |
| GPU Ringbuf (medium) | medium | 35.68 | 27.46 | 1.30x (+29.9%) |


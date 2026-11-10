
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T18:34:44.981163  

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
| Baseline (tiny) | tiny | 27.03 | - | - |
| Baseline (small) | small | 27.29 | - | - |
| Baseline (medium) | medium | 27.75 | - | - |
| Baseline (large) | large | 31.32 | - | - |
| Baseline (xlarge) | xlarge | 159.26 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 31.98 | 27.29 | 1.17x (+17.2%) |
| Entry+Exit (small) | small | 35.90 | 27.29 | 1.32x (+31.6%) |
| Entry+Exit (large) | large | 41.28 | 31.32 | 1.32x (+31.8%) |
| Empty probe (large) | large | 39.31 | 31.32 | 1.26x (+25.5%) |
| Empty probe (xlarge) | xlarge | 174.58 | 159.26 | 1.10x (+9.6%) |
| Entry probe (xlarge) | xlarge | 171.54 | 159.26 | 1.08x (+7.7%) |
| Memtrace (tiny) | tiny | 35.93 | 27.03 | 1.33x (+32.9%) |
| Memtrace (large) | large | 172.71 | 31.32 | 5.51x (+451.4%) |
| GPU Array map lookup (xlarge) | xlarge | 440.72 | 159.26 | 2.77x (+176.7%) |
| Per-GPU-thread array (tiny) | tiny | 37.11 | 27.03 | 1.37x (+37.3%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 37.53 | 27.29 | 1.38x (+37.5%) |
| GPU Array map update (small_64x16) | small_64x16 | 40.20 | 27.29 | 1.47x (+47.3%) |
| GPU Array map update (small) | small | 34.30 | 27.29 | 1.26x (+25.7%) |
| GPU Array map update (large) | large | 245.67 | 31.32 | 7.84x (+684.4%) |
| GPU Array map update (xlarge) | xlarge | 1366.85 | 159.26 | 8.58x (+758.3%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 47.43 | 27.29 | 1.74x (+73.8%) |
| GPU Ringbuf (small) | small | 39.65 | 27.29 | 1.45x (+45.3%) |
| GPU Ringbuf (medium) | medium | 43.90 | 27.75 | 1.58x (+58.2%) |


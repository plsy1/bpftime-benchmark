
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T18:19:42.898183  

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
| Baseline (tiny) | tiny | 27.41 | - | - |
| Baseline (small) | small | 27.34 | - | - |
| Baseline (medium) | medium | 28.27 | - | - |
| Baseline (large) | large | 31.08 | - | - |
| Baseline (xlarge) | xlarge | 160.02 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 32.03 | 27.34 | 1.17x (+17.2%) |
| Entry+Exit (small) | small | 36.14 | 27.34 | 1.32x (+32.2%) |
| Entry+Exit (large) | large | 37.08 | 31.08 | 1.19x (+19.3%) |
| Empty probe (large) | large | 43.81 | 31.08 | 1.41x (+41.0%) |
| Empty probe (xlarge) | xlarge | 169.52 | 160.02 | 1.06x (+5.9%) |
| Entry probe (xlarge) | xlarge | 167.69 | 160.02 | 1.05x (+4.8%) |
| Memtrace (tiny) | tiny | 32.99 | 27.41 | 1.20x (+20.4%) |
| Memtrace (large) | large | 176.26 | 31.08 | 5.67x (+467.1%) |
| GPU Array map lookup (xlarge) | xlarge | 455.09 | 160.02 | 2.84x (+184.4%) |
| Per-GPU-thread array (tiny) | tiny | 33.83 | 27.41 | 1.23x (+23.4%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 36.15 | 27.34 | 1.32x (+32.2%) |
| GPU Array map update (small_64x16) | small_64x16 | 35.36 | 27.34 | 1.29x (+29.3%) |
| GPU Array map update (small) | small | 35.51 | 27.34 | 1.30x (+29.9%) |
| GPU Array map update (large) | large | 237.05 | 31.08 | 7.63x (+662.7%) |
| GPU Array map update (xlarge) | xlarge | 1543.98 | 160.02 | 9.65x (+864.9%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 45.93 | 27.34 | 1.68x (+68.0%) |
| GPU Ringbuf (small) | small | 46.15 | 27.34 | 1.69x (+68.8%) |
| GPU Ringbuf (medium) | medium | 41.35 | 28.27 | 1.46x (+46.3%) |


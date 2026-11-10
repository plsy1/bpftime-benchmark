
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T15:55:06.587304  

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
| Baseline (tiny) | tiny | 27.75 | - | - |
| Baseline (small) | small | 27.18 | - | - |
| Baseline (medium) | medium | 27.56 | - | - |
| Baseline (large) | large | 31.29 | - | - |
| Baseline (xlarge) | xlarge | 160.16 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 30.07 | 27.18 | 1.11x (+10.6%) |
| Entry+Exit (small) | small | 32.13 | 27.18 | 1.18x (+18.2%) |
| Entry+Exit (large) | large | 45.40 | 31.29 | 1.45x (+45.1%) |
| Empty probe (large) | large | 47.25 | 31.29 | 1.51x (+51.0%) |
| Empty probe (xlarge) | xlarge | 178.56 | 160.16 | 1.11x (+11.5%) |
| Entry probe (xlarge) | xlarge | 175.05 | 160.16 | 1.09x (+9.3%) |
| Memtrace (tiny) | tiny | 36.03 | 27.75 | 1.30x (+29.8%) |
| Memtrace (large) | large | 178.18 | 31.29 | 5.69x (+469.4%) |
| GPU Array map lookup (xlarge) | xlarge | 455.42 | 160.16 | 2.84x (+184.4%) |
| Per-GPU-thread array (tiny) | tiny | 39.54 | 27.75 | 1.42x (+42.5%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 40.74 | 27.18 | 1.50x (+49.9%) |
| GPU Array map update (small_64x16) | small_64x16 | 41.80 | 27.18 | 1.54x (+53.8%) |
| GPU Array map update (small) | small | 40.70 | 27.18 | 1.50x (+49.7%) |
| GPU Array map update (large) | large | 219.48 | 31.29 | 7.01x (+601.4%) |
| GPU Array map update (xlarge) | xlarge | 1707.48 | 160.16 | 10.66x (+966.1%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 37.77 | 27.18 | 1.39x (+39.0%) |
| GPU Ringbuf (small) | small | 38.36 | 27.18 | 1.41x (+41.1%) |
| GPU Ringbuf (medium) | medium | 36.98 | 27.56 | 1.34x (+34.2%) |



# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T17:09:35.986896  

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
| Baseline (tiny) | tiny | 27.13 | - | - |
| Baseline (small) | small | 27.22 | - | - |
| Baseline (medium) | medium | 27.29 | - | - |
| Baseline (large) | large | 32.67 | - | - |
| Baseline (xlarge) | xlarge | 159.63 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 35.35 | 27.22 | 1.30x (+29.9%) |
| Entry+Exit (small) | small | 32.74 | 27.22 | 1.20x (+20.3%) |
| Entry+Exit (large) | large | 42.05 | 32.67 | 1.29x (+28.7%) |
| Empty probe (large) | large | 40.32 | 32.67 | 1.23x (+23.4%) |
| Empty probe (xlarge) | xlarge | 180.52 | 159.63 | 1.13x (+13.1%) |
| Entry probe (xlarge) | xlarge | 172.35 | 159.63 | 1.08x (+8.0%) |
| Memtrace (tiny) | tiny | 36.33 | 27.13 | 1.34x (+33.9%) |
| Memtrace (large) | large | 179.86 | 32.67 | 5.51x (+450.5%) |
| GPU Array map lookup (xlarge) | xlarge | 449.13 | 159.63 | 2.81x (+181.4%) |
| Per-GPU-thread array (tiny) | tiny | 38.53 | 27.13 | 1.42x (+42.0%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 39.59 | 27.22 | 1.45x (+45.4%) |
| GPU Array map update (small_64x16) | small_64x16 | 42.05 | 27.22 | 1.54x (+54.5%) |
| GPU Array map update (small) | small | 41.14 | 27.22 | 1.51x (+51.1%) |
| GPU Array map update (large) | large | 215.19 | 32.67 | 6.59x (+558.7%) |
| GPU Array map update (xlarge) | xlarge | 1677.29 | 159.63 | 10.51x (+950.7%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 39.79 | 27.22 | 1.46x (+46.2%) |
| GPU Ringbuf (small) | small | 36.12 | 27.22 | 1.33x (+32.7%) |
| GPU Ringbuf (medium) | medium | 34.64 | 27.29 | 1.27x (+26.9%) |


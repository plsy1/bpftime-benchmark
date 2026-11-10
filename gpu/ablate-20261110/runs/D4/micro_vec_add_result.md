
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T16:36:42.521008  

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
| Baseline (tiny) | tiny | 27.32 | - | - |
| Baseline (small) | small | 27.50 | - | - |
| Baseline (medium) | medium | 25.63 | - | - |
| Baseline (large) | large | 31.44 | - | - |
| Baseline (xlarge) | xlarge | 160.89 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 28.70 | 27.50 | 1.04x (+4.4%) |
| Entry+Exit (small) | small | 32.38 | 27.50 | 1.18x (+17.7%) |
| Entry+Exit (large) | large | 40.63 | 31.44 | 1.29x (+29.2%) |
| Empty probe (large) | large | 36.02 | 31.44 | 1.15x (+14.6%) |
| Empty probe (xlarge) | xlarge | 171.07 | 160.89 | 1.06x (+6.3%) |
| Entry probe (xlarge) | xlarge | 174.35 | 160.89 | 1.08x (+8.4%) |
| Memtrace (tiny) | tiny | 29.20 | 27.32 | 1.07x (+6.9%) |
| Memtrace (large) | large | 175.15 | 31.44 | 5.57x (+457.1%) |
| GPU Array map lookup (xlarge) | xlarge | 444.90 | 160.89 | 2.77x (+176.5%) |
| Per-GPU-thread array (tiny) | tiny | 34.92 | 27.32 | 1.28x (+27.8%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 37.64 | 27.50 | 1.37x (+36.9%) |
| GPU Array map update (small_64x16) | small_64x16 | 34.43 | 27.50 | 1.25x (+25.2%) |
| GPU Array map update (small) | small | 33.91 | 27.50 | 1.23x (+23.3%) |
| GPU Array map update (large) | large | 243.23 | 31.44 | 7.74x (+673.6%) |
| GPU Array map update (xlarge) | xlarge | 1546.60 | 160.89 | 9.61x (+861.3%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 47.06 | 27.50 | 1.71x (+71.1%) |
| GPU Ringbuf (small) | small | 42.18 | 27.50 | 1.53x (+53.4%) |
| GPU Ringbuf (medium) | medium | 40.82 | 25.63 | 1.59x (+59.3%) |


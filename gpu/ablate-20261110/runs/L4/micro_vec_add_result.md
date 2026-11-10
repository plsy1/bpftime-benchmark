
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T16:40:16.234940  

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
| Baseline (tiny) | tiny | 27.10 | - | - |
| Baseline (small) | small | 28.05 | - | - |
| Baseline (medium) | medium | 26.62 | - | - |
| Baseline (large) | large | 31.09 | - | - |
| Baseline (xlarge) | xlarge | 161.20 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 34.57 | 28.05 | 1.23x (+23.2%) |
| Entry+Exit (small) | small | 40.10 | 28.05 | 1.43x (+43.0%) |
| Entry+Exit (large) | large | 49.67 | 31.09 | 1.60x (+59.8%) |
| Empty probe (large) | large | 51.51 | 31.09 | 1.66x (+65.7%) |
| Empty probe (xlarge) | xlarge | 182.23 | 161.20 | 1.13x (+13.0%) |
| Entry probe (xlarge) | xlarge | 176.91 | 161.20 | 1.10x (+9.7%) |
| Memtrace (tiny) | tiny | 42.52 | 27.10 | 1.57x (+56.9%) |
| Memtrace (large) | large | 186.58 | 31.09 | 6.00x (+500.1%) |
| GPU Array map lookup (xlarge) | xlarge | 454.52 | 161.20 | 2.82x (+182.0%) |
| Per-GPU-thread array (tiny) | tiny | 45.58 | 27.10 | 1.68x (+68.2%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 47.24 | 28.05 | 1.68x (+68.4%) |
| GPU Array map update (small_64x16) | small_64x16 | 46.72 | 28.05 | 1.67x (+66.6%) |
| GPU Array map update (small) | small | 47.86 | 28.05 | 1.71x (+70.6%) |
| GPU Array map update (large) | large | 250.91 | 31.09 | 8.07x (+707.0%) |
| GPU Array map update (xlarge) | xlarge | 1513.94 | 161.20 | 9.39x (+839.2%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 58.15 | 28.05 | 2.07x (+107.3%) |
| GPU Ringbuf (small) | small | 58.45 | 28.05 | 2.08x (+108.4%) |
| GPU Ringbuf (medium) | medium | 54.97 | 26.62 | 2.06x (+106.5%) |


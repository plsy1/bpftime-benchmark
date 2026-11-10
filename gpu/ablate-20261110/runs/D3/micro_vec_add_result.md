
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T16:18:06.358923  

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
| Baseline (tiny) | tiny | 27.14 | - | - |
| Baseline (small) | small | 27.06 | - | - |
| Baseline (medium) | medium | 27.34 | - | - |
| Baseline (large) | large | 31.15 | - | - |
| Baseline (xlarge) | xlarge | 161.15 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 30.62 | 27.06 | 1.13x (+13.2%) |
| Entry+Exit (small) | small | 36.01 | 27.06 | 1.33x (+33.1%) |
| Entry+Exit (large) | large | 40.55 | 31.15 | 1.30x (+30.2%) |
| Empty probe (large) | large | 35.69 | 31.15 | 1.15x (+14.6%) |
| Empty probe (xlarge) | xlarge | 177.08 | 161.15 | 1.10x (+9.9%) |
| Entry probe (xlarge) | xlarge | 171.66 | 161.15 | 1.07x (+6.5%) |
| Memtrace (tiny) | tiny | 35.74 | 27.14 | 1.32x (+31.7%) |
| Memtrace (large) | large | 173.37 | 31.15 | 5.57x (+456.6%) |
| GPU Array map lookup (xlarge) | xlarge | 455.50 | 161.15 | 2.83x (+182.7%) |
| Per-GPU-thread array (tiny) | tiny | 36.14 | 27.14 | 1.33x (+33.2%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 35.12 | 27.06 | 1.30x (+29.8%) |
| GPU Array map update (small_64x16) | small_64x16 | 40.41 | 27.06 | 1.49x (+49.3%) |
| GPU Array map update (small) | small | 37.35 | 27.06 | 1.38x (+38.0%) |
| GPU Array map update (large) | large | 252.51 | 31.15 | 8.11x (+710.6%) |
| GPU Array map update (xlarge) | xlarge | 1520.59 | 161.15 | 9.44x (+843.6%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 42.24 | 27.06 | 1.56x (+56.1%) |
| GPU Ringbuf (small) | small | 40.05 | 27.06 | 1.48x (+48.0%) |
| GPU Ringbuf (medium) | medium | 45.30 | 27.34 | 1.66x (+65.7%) |


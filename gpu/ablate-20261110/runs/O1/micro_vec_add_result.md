
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T15:15:08.497012  

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
| Baseline (tiny) | tiny | 30.18 | - | - |
| Baseline (small) | small | 27.77 | - | - |
| Baseline (medium) | medium | 28.39 | - | - |
| Baseline (large) | large | 31.42 | - | - |
| Baseline (xlarge) | xlarge | 158.94 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 30.57 | 27.77 | 1.10x (+10.1%) |
| Entry+Exit (small) | small | 30.69 | 27.77 | 1.11x (+10.5%) |
| Entry+Exit (large) | large | 45.41 | 31.42 | 1.45x (+44.5%) |
| Empty probe (large) | large | 46.92 | 31.42 | 1.49x (+49.3%) |
| Empty probe (xlarge) | xlarge | 177.95 | 158.94 | 1.12x (+12.0%) |
| Entry probe (xlarge) | xlarge | 174.78 | 158.94 | 1.10x (+10.0%) |
| Memtrace (tiny) | tiny | 37.66 | 30.18 | 1.25x (+24.8%) |
| Memtrace (large) | large | 175.65 | 31.42 | 5.59x (+459.0%) |
| GPU Array map lookup (xlarge) | xlarge | 457.22 | 158.94 | 2.88x (+187.7%) |
| Per-GPU-thread array (tiny) | tiny | 39.30 | 30.18 | 1.30x (+30.2%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 38.97 | 27.77 | 1.40x (+40.3%) |
| GPU Array map update (small_64x16) | small_64x16 | 41.02 | 27.77 | 1.48x (+47.7%) |
| GPU Array map update (small) | small | 41.34 | 27.77 | 1.49x (+48.9%) |
| GPU Array map update (large) | large | 223.74 | 31.42 | 7.12x (+612.1%) |
| GPU Array map update (xlarge) | xlarge | 1718.72 | 158.94 | 10.81x (+981.4%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 39.99 | 27.77 | 1.44x (+44.0%) |
| GPU Ringbuf (small) | small | 38.12 | 27.77 | 1.37x (+37.3%) |
| GPU Ringbuf (medium) | medium | 38.61 | 28.39 | 1.36x (+36.0%) |


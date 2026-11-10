
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T18:26:50.489172  

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
| Baseline (tiny) | tiny | 27.55 | - | - |
| Baseline (small) | small | 26.65 | - | - |
| Baseline (medium) | medium | 26.97 | - | - |
| Baseline (large) | large | 30.87 | - | - |
| Baseline (xlarge) | xlarge | 161.49 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 35.67 | 26.65 | 1.34x (+33.8%) |
| Entry+Exit (small) | small | 30.03 | 26.65 | 1.13x (+12.7%) |
| Entry+Exit (large) | large | 43.39 | 30.87 | 1.41x (+40.6%) |
| Empty probe (large) | large | 47.89 | 30.87 | 1.55x (+55.1%) |
| Empty probe (xlarge) | xlarge | 177.96 | 161.49 | 1.10x (+10.2%) |
| Entry probe (xlarge) | xlarge | 171.37 | 161.49 | 1.06x (+6.1%) |
| Memtrace (tiny) | tiny | 31.40 | 27.55 | 1.14x (+14.0%) |
| Memtrace (large) | large | 175.87 | 30.87 | 5.70x (+469.7%) |
| GPU Array map lookup (xlarge) | xlarge | 451.48 | 161.49 | 2.80x (+179.6%) |
| Per-GPU-thread array (tiny) | tiny | 36.49 | 27.55 | 1.32x (+32.5%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 38.24 | 26.65 | 1.43x (+43.5%) |
| GPU Array map update (small_64x16) | small_64x16 | 40.02 | 26.65 | 1.50x (+50.2%) |
| GPU Array map update (small) | small | 41.28 | 26.65 | 1.55x (+54.9%) |
| GPU Array map update (large) | large | 221.25 | 30.87 | 7.17x (+616.7%) |
| GPU Array map update (xlarge) | xlarge | 1664.99 | 161.49 | 10.31x (+931.0%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 38.73 | 26.65 | 1.45x (+45.3%) |
| GPU Ringbuf (small) | small | 36.02 | 26.65 | 1.35x (+35.2%) |
| GPU Ringbuf (medium) | medium | 34.87 | 26.97 | 1.29x (+29.3%) |


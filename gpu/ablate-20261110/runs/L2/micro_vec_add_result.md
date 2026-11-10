
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T15:40:50.562418  

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
| Baseline (tiny) | tiny | 27.67 | - | - |
| Baseline (small) | small | 26.15 | - | - |
| Baseline (medium) | medium | 27.08 | - | - |
| Baseline (large) | large | 31.13 | - | - |
| Baseline (xlarge) | xlarge | 159.24 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 36.61 | 26.15 | 1.40x (+40.0%) |
| Entry+Exit (small) | small | 31.57 | 26.15 | 1.21x (+20.7%) |
| Entry+Exit (large) | large | 51.04 | 31.13 | 1.64x (+64.0%) |
| Empty probe (large) | large | 52.91 | 31.13 | 1.70x (+70.0%) |
| Empty probe (xlarge) | xlarge | 182.76 | 159.24 | 1.15x (+14.8%) |
| Entry probe (xlarge) | xlarge | 183.02 | 159.24 | 1.15x (+14.9%) |
| Memtrace (tiny) | tiny | 39.76 | 27.67 | 1.44x (+43.7%) |
| Memtrace (large) | large | 181.64 | 31.13 | 5.83x (+483.5%) |
| GPU Array map lookup (xlarge) | xlarge | 455.39 | 159.24 | 2.86x (+186.0%) |
| Per-GPU-thread array (tiny) | tiny | 47.02 | 27.67 | 1.70x (+69.9%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 48.10 | 26.15 | 1.84x (+83.9%) |
| GPU Array map update (small_64x16) | small_64x16 | 50.89 | 26.15 | 1.95x (+94.6%) |
| GPU Array map update (small) | small | 48.33 | 26.15 | 1.85x (+84.8%) |
| GPU Array map update (large) | large | 246.91 | 31.13 | 7.93x (+693.2%) |
| GPU Array map update (xlarge) | xlarge | 1495.94 | 159.24 | 9.39x (+839.4%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 55.92 | 26.15 | 2.14x (+113.8%) |
| GPU Ringbuf (small) | small | 55.33 | 26.15 | 2.12x (+111.6%) |
| GPU Ringbuf (medium) | medium | 52.32 | 27.08 | 1.93x (+93.2%) |


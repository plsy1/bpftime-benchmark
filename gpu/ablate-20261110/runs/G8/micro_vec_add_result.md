
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T18:01:05.373427  

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
| Baseline (tiny) | tiny | 27.53 | - | - |
| Baseline (small) | small | 27.04 | - | - |
| Baseline (medium) | medium | 27.45 | - | - |
| Baseline (large) | large | 31.59 | - | - |
| Baseline (xlarge) | xlarge | 160.24 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 31.89 | 27.04 | 1.18x (+17.9%) |
| Entry+Exit (small) | small | 34.51 | 27.04 | 1.28x (+27.6%) |
| Entry+Exit (large) | large | 38.14 | 31.59 | 1.21x (+20.7%) |
| Empty probe (large) | large | 42.53 | 31.59 | 1.35x (+34.6%) |
| Empty probe (xlarge) | xlarge | 169.86 | 160.24 | 1.06x (+6.0%) |
| Entry probe (xlarge) | xlarge | 172.19 | 160.24 | 1.07x (+7.5%) |
| Memtrace (tiny) | tiny | 34.42 | 27.53 | 1.25x (+25.0%) |
| Memtrace (large) | large | 179.32 | 31.59 | 5.68x (+467.6%) |
| GPU Array map lookup (xlarge) | xlarge | 453.93 | 160.24 | 2.83x (+183.3%) |
| Per-GPU-thread array (tiny) | tiny | 34.67 | 27.53 | 1.26x (+25.9%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 34.73 | 27.04 | 1.28x (+28.4%) |
| GPU Array map update (small_64x16) | small_64x16 | 35.11 | 27.04 | 1.30x (+29.8%) |
| GPU Array map update (small) | small | 35.52 | 27.04 | 1.31x (+31.4%) |
| GPU Array map update (large) | large | 245.82 | 31.59 | 7.78x (+678.2%) |
| GPU Array map update (xlarge) | xlarge | 1585.71 | 160.24 | 9.90x (+889.6%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 39.97 | 27.04 | 1.48x (+47.8%) |
| GPU Ringbuf (small) | small | 46.21 | 27.04 | 1.71x (+70.9%) |
| GPU Ringbuf (medium) | medium | 47.60 | 27.45 | 1.73x (+73.4%) |



# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T18:08:12.408399  

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
| Baseline (small) | small | 27.62 | - | - |
| Baseline (medium) | medium | 27.54 | - | - |
| Baseline (large) | large | 31.16 | - | - |
| Baseline (xlarge) | xlarge | 159.39 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 36.07 | 27.62 | 1.31x (+30.6%) |
| Entry+Exit (small) | small | 33.44 | 27.62 | 1.21x (+21.1%) |
| Entry+Exit (large) | large | 46.16 | 31.16 | 1.48x (+48.1%) |
| Empty probe (large) | large | 42.90 | 31.16 | 1.38x (+37.7%) |
| Empty probe (xlarge) | xlarge | 175.02 | 159.39 | 1.10x (+9.8%) |
| Entry probe (xlarge) | xlarge | 172.45 | 159.39 | 1.08x (+8.2%) |
| Memtrace (tiny) | tiny | 37.45 | 27.32 | 1.37x (+37.1%) |
| Memtrace (large) | large | 177.47 | 31.16 | 5.70x (+469.5%) |
| GPU Array map lookup (xlarge) | xlarge | 458.51 | 159.39 | 2.88x (+187.7%) |
| Per-GPU-thread array (tiny) | tiny | 38.82 | 27.32 | 1.42x (+42.1%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 39.49 | 27.62 | 1.43x (+43.0%) |
| GPU Array map update (small_64x16) | small_64x16 | 41.44 | 27.62 | 1.50x (+50.0%) |
| GPU Array map update (small) | small | 41.39 | 27.62 | 1.50x (+49.9%) |
| GPU Array map update (large) | large | 218.82 | 31.16 | 7.02x (+602.2%) |
| GPU Array map update (xlarge) | xlarge | 1698.64 | 159.39 | 10.66x (+965.7%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 33.75 | 27.62 | 1.22x (+22.2%) |
| GPU Ringbuf (small) | small | 39.91 | 27.62 | 1.44x (+44.5%) |
| GPU Ringbuf (medium) | medium | 35.96 | 27.54 | 1.31x (+30.6%) |



# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T17:57:31.542584  

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
| Baseline (tiny) | tiny | 27.48 | - | - |
| Baseline (small) | small | 27.43 | - | - |
| Baseline (medium) | medium | 27.17 | - | - |
| Baseline (large) | large | 31.32 | - | - |
| Baseline (xlarge) | xlarge | 159.55 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 33.53 | 27.43 | 1.22x (+22.2%) |
| Entry+Exit (small) | small | 32.27 | 27.43 | 1.18x (+17.6%) |
| Entry+Exit (large) | large | 44.23 | 31.32 | 1.41x (+41.2%) |
| Empty probe (large) | large | 43.19 | 31.32 | 1.38x (+37.9%) |
| Empty probe (xlarge) | xlarge | 172.60 | 159.55 | 1.08x (+8.2%) |
| Entry probe (xlarge) | xlarge | 167.28 | 159.55 | 1.05x (+4.8%) |
| Memtrace (tiny) | tiny | 30.17 | 27.48 | 1.10x (+9.8%) |
| Memtrace (large) | large | 176.47 | 31.32 | 5.63x (+463.4%) |
| GPU Array map lookup (xlarge) | xlarge | 445.71 | 159.55 | 2.79x (+179.4%) |
| Per-GPU-thread array (tiny) | tiny | 34.79 | 27.48 | 1.27x (+26.6%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 34.19 | 27.43 | 1.25x (+24.6%) |
| GPU Array map update (small_64x16) | small_64x16 | 36.21 | 27.43 | 1.32x (+32.0%) |
| GPU Array map update (small) | small | 34.21 | 27.43 | 1.25x (+24.7%) |
| GPU Array map update (large) | large | 234.33 | 31.32 | 7.48x (+648.2%) |
| GPU Array map update (xlarge) | xlarge | 1516.21 | 159.55 | 9.50x (+850.3%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 43.06 | 27.43 | 1.57x (+57.0%) |
| GPU Ringbuf (small) | small | 48.65 | 27.43 | 1.77x (+77.4%) |
| GPU Ringbuf (medium) | medium | 41.31 | 27.17 | 1.52x (+52.0%) |


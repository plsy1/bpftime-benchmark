
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T16:03:02.363639  

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
| Baseline (tiny) | tiny | 28.03 | - | - |
| Baseline (small) | small | 27.28 | - | - |
| Baseline (medium) | medium | 26.25 | - | - |
| Baseline (large) | large | 31.21 | - | - |
| Baseline (xlarge) | xlarge | 160.36 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 36.25 | 27.28 | 1.33x (+32.9%) |
| Entry+Exit (small) | small | 33.96 | 27.28 | 1.24x (+24.5%) |
| Entry+Exit (large) | large | 42.96 | 31.21 | 1.38x (+37.6%) |
| Empty probe (large) | large | 42.92 | 31.21 | 1.38x (+37.5%) |
| Empty probe (xlarge) | xlarge | 172.69 | 160.36 | 1.08x (+7.7%) |
| Entry probe (xlarge) | xlarge | 166.80 | 160.36 | 1.04x (+4.0%) |
| Memtrace (tiny) | tiny | 37.61 | 28.03 | 1.34x (+34.2%) |
| Memtrace (large) | large | 175.76 | 31.21 | 5.63x (+463.2%) |
| GPU Array map lookup (xlarge) | xlarge | 438.60 | 160.36 | 2.74x (+173.5%) |
| Per-GPU-thread array (tiny) | tiny | 39.00 | 28.03 | 1.39x (+39.1%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 32.57 | 27.28 | 1.19x (+19.4%) |
| GPU Array map update (small_64x16) | small_64x16 | 37.57 | 27.28 | 1.38x (+37.7%) |
| GPU Array map update (small) | small | 38.90 | 27.28 | 1.43x (+42.6%) |
| GPU Array map update (large) | large | 249.53 | 31.21 | 8.00x (+699.5%) |
| GPU Array map update (xlarge) | xlarge | 1472.47 | 160.36 | 9.18x (+818.2%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 42.00 | 27.28 | 1.54x (+54.0%) |
| GPU Ringbuf (small) | small | 44.42 | 27.28 | 1.63x (+62.8%) |
| GPU Ringbuf (medium) | medium | 47.07 | 26.25 | 1.79x (+79.3%) |


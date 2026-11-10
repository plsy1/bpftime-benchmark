
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T15:47:59.585111  

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
| Baseline (tiny) | tiny | 27.24 | - | - |
| Baseline (small) | small | 27.37 | - | - |
| Baseline (medium) | medium | 27.32 | - | - |
| Baseline (large) | large | 31.56 | - | - |
| Baseline (xlarge) | xlarge | 160.70 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 36.69 | 27.37 | 1.34x (+34.1%) |
| Entry+Exit (small) | small | 35.12 | 27.37 | 1.28x (+28.3%) |
| Entry+Exit (large) | large | 41.85 | 31.56 | 1.33x (+32.6%) |
| Empty probe (large) | large | 36.68 | 31.56 | 1.16x (+16.2%) |
| Empty probe (xlarge) | xlarge | 167.86 | 160.70 | 1.04x (+4.5%) |
| Entry probe (xlarge) | xlarge | 168.50 | 160.70 | 1.05x (+4.9%) |
| Memtrace (tiny) | tiny | 37.16 | 27.24 | 1.36x (+36.4%) |
| Memtrace (large) | large | 173.31 | 31.56 | 5.49x (+449.1%) |
| GPU Array map lookup (xlarge) | xlarge | 446.46 | 160.70 | 2.78x (+177.8%) |
| Per-GPU-thread array (tiny) | tiny | 37.41 | 27.24 | 1.37x (+37.3%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 33.77 | 27.37 | 1.23x (+23.4%) |
| GPU Array map update (small_64x16) | small_64x16 | 39.06 | 27.37 | 1.43x (+42.7%) |
| GPU Array map update (small) | small | 35.51 | 27.37 | 1.30x (+29.7%) |
| GPU Array map update (large) | large | 242.96 | 31.56 | 7.70x (+669.8%) |
| GPU Array map update (xlarge) | xlarge | 1564.58 | 160.70 | 9.74x (+873.6%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 44.24 | 27.37 | 1.62x (+61.6%) |
| GPU Ringbuf (small) | small | 44.09 | 27.37 | 1.61x (+61.1%) |
| GPU Ringbuf (medium) | medium | 46.97 | 27.32 | 1.72x (+71.9%) |


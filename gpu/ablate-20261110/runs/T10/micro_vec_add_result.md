
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T18:41:53.161268  

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
| Baseline (tiny) | tiny | 26.82 | - | - |
| Baseline (small) | small | 27.11 | - | - |
| Baseline (medium) | medium | 27.46 | - | - |
| Baseline (large) | large | 31.20 | - | - |
| Baseline (xlarge) | xlarge | 160.47 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 36.22 | 27.11 | 1.34x (+33.6%) |
| Entry+Exit (small) | small | 32.06 | 27.11 | 1.18x (+18.3%) |
| Entry+Exit (large) | large | 43.01 | 31.20 | 1.38x (+37.9%) |
| Empty probe (large) | large | 36.62 | 31.20 | 1.17x (+17.4%) |
| Empty probe (xlarge) | xlarge | 170.59 | 160.47 | 1.06x (+6.3%) |
| Entry probe (xlarge) | xlarge | 169.29 | 160.47 | 1.05x (+5.5%) |
| Memtrace (tiny) | tiny | 28.57 | 26.82 | 1.07x (+6.5%) |
| Memtrace (large) | large | 171.86 | 31.20 | 5.51x (+450.8%) |
| GPU Array map lookup (xlarge) | xlarge | 440.41 | 160.47 | 2.74x (+174.5%) |
| Per-GPU-thread array (tiny) | tiny | 37.54 | 26.82 | 1.40x (+40.0%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 39.30 | 27.11 | 1.45x (+45.0%) |
| GPU Array map update (small_64x16) | small_64x16 | 35.62 | 27.11 | 1.31x (+31.4%) |
| GPU Array map update (small) | small | 32.35 | 27.11 | 1.19x (+19.3%) |
| GPU Array map update (large) | large | 226.34 | 31.20 | 7.25x (+625.4%) |
| GPU Array map update (xlarge) | xlarge | 1451.69 | 160.47 | 9.05x (+804.6%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 31.87 | 27.11 | 1.18x (+17.6%) |
| GPU Ringbuf (small) | small | 33.26 | 27.11 | 1.23x (+22.7%) |
| GPU Ringbuf (medium) | medium | 36.00 | 27.46 | 1.31x (+31.1%) |


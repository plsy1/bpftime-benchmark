
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T15:19:29.391658  

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
| Baseline (small) | small | 26.17 | - | - |
| Baseline (medium) | medium | 27.70 | - | - |
| Baseline (large) | large | 31.35 | - | - |
| Baseline (xlarge) | xlarge | 159.18 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 32.81 | 26.17 | 1.25x (+25.4%) |
| Entry+Exit (small) | small | 29.36 | 26.17 | 1.12x (+12.2%) |
| Entry+Exit (large) | large | 42.12 | 31.35 | 1.34x (+34.4%) |
| Empty probe (large) | large | 36.40 | 31.35 | 1.16x (+16.1%) |
| Empty probe (xlarge) | xlarge | 173.75 | 159.18 | 1.09x (+9.2%) |
| Entry probe (xlarge) | xlarge | 167.16 | 159.18 | 1.05x (+5.0%) |
| Memtrace (tiny) | tiny | 34.03 | 27.24 | 1.25x (+24.9%) |
| Memtrace (large) | large | 171.81 | 31.35 | 5.48x (+448.0%) |
| GPU Array map lookup (xlarge) | xlarge | 442.59 | 159.18 | 2.78x (+178.0%) |
| Per-GPU-thread array (tiny) | tiny | 38.29 | 27.24 | 1.41x (+40.6%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 35.96 | 26.17 | 1.37x (+37.4%) |
| GPU Array map update (small_64x16) | small_64x16 | 37.94 | 26.17 | 1.45x (+45.0%) |
| GPU Array map update (small) | small | 37.72 | 26.17 | 1.44x (+44.1%) |
| GPU Array map update (large) | large | 246.96 | 31.35 | 7.88x (+687.8%) |
| GPU Array map update (xlarge) | xlarge | 1564.03 | 159.18 | 9.83x (+882.6%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 45.30 | 26.17 | 1.73x (+73.1%) |
| GPU Ringbuf (small) | small | 42.33 | 26.17 | 1.62x (+61.8%) |
| GPU Ringbuf (medium) | medium | 47.19 | 27.70 | 1.70x (+70.4%) |


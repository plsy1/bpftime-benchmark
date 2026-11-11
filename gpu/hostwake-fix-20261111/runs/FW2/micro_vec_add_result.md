
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:14:18.264965  

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
| Baseline (tiny) | tiny | 27.19 | - | - |
| Baseline (small) | small | 27.71 | - | - |
| Baseline (large) | large | 31.10 | - | - |
| Baseline (xlarge) | xlarge | 159.98 | - | - |
| Baseline (minimal) | minimal | 41.63 | - | - |
| Empty probe (tiny) | tiny | 27.37 | 27.19 | 1.01x (+0.7%) |
| Empty probe (xlarge) | xlarge | 177.43 | 159.98 | 1.11x (+10.9%) |
| Global timer (xlarge) | xlarge | 181.25 | 159.98 | 1.13x (+13.3%) |
| GPU Ringbuf (small) | small | 44.46 | 27.71 | 1.60x (+60.4%) |
| GPU Ringbuf (xlarge) | xlarge | 215.09 | 159.98 | 1.34x (+34.4%) |
| Per-GPU-thread array (small) | small | 36.36 | 27.71 | 1.31x (+31.2%) |
| Memtrace (small) | small | 36.49 | 27.71 | 1.32x (+31.7%) |
| Memtrace (large) | large | 175.45 | 31.10 | 5.64x (+464.1%) |
| GPU Array map update (small) | small | 39.10 | 27.71 | 1.41x (+41.1%) |
| GPU Array map update (large) | large | 226.45 | 31.10 | 7.28x (+628.1%) |
| GPU Array map lookup (small) | small | 33.66 | 27.71 | 1.21x (+21.5%) |
| GPU Array map lookup (large) | large | 69.39 | 31.10 | 2.23x (+123.1%) |
| CPU Array map update (minimal) | minimal | 43.98 | 27.19 | 1.62x (+61.8%) |
| CPU Array map lookup (minimal) | minimal | 45.27 | 27.19 | 1.66x (+66.5%) |
| CUDA Counter (minimal) | minimal | 2900.18 | 41.63 | 69.67x (+6866.6%) |
| Kernel Retsnoop (minimal) | minimal | 57.51 | 41.63 | 1.38x (+38.1%) |
| Thread Histogram (minimal) | minimal | 46.36 | 41.63 | 1.11x (+11.4%) |
| Memory Trace (minimal) | minimal | 66.30 | 41.63 | 1.59x (+59.3%) |


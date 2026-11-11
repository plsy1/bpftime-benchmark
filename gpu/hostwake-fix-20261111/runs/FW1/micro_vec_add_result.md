
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:13:54.133504  

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
| Baseline (tiny) | tiny | 27.46 | - | - |
| Baseline (small) | small | 27.50 | - | - |
| Baseline (large) | large | 31.08 | - | - |
| Baseline (xlarge) | xlarge | 159.31 | - | - |
| Baseline (minimal) | minimal | 38.36 | - | - |
| Empty probe (tiny) | tiny | 27.78 | 27.46 | 1.01x (+1.2%) |
| Empty probe (xlarge) | xlarge | 174.07 | 159.31 | 1.09x (+9.3%) |
| Global timer (xlarge) | xlarge | 177.24 | 159.31 | 1.11x (+11.3%) |
| GPU Ringbuf (small) | small | 46.84 | 27.50 | 1.70x (+70.3%) |
| GPU Ringbuf (xlarge) | xlarge | 220.61 | 159.31 | 1.38x (+38.5%) |
| Per-GPU-thread array (small) | small | 37.54 | 27.50 | 1.37x (+36.5%) |
| Memtrace (small) | small | 37.81 | 27.50 | 1.37x (+37.5%) |
| Memtrace (large) | large | 175.20 | 31.08 | 5.64x (+463.7%) |
| GPU Array map update (small) | small | 37.29 | 27.50 | 1.36x (+35.6%) |
| GPU Array map update (large) | large | 235.54 | 31.08 | 7.58x (+657.9%) |
| GPU Array map lookup (small) | small | 35.32 | 27.50 | 1.28x (+28.4%) |
| GPU Array map lookup (large) | large | 69.80 | 31.08 | 2.25x (+124.6%) |
| CPU Array map update (minimal) | minimal | 37.95 | 27.46 | 1.38x (+38.2%) |
| CPU Array map lookup (minimal) | minimal | 37.58 | 27.46 | 1.37x (+36.9%) |
| CUDA Counter (minimal) | minimal | 2933.28 | 38.36 | 76.47x (+7546.7%) |
| Kernel Retsnoop (minimal) | minimal | 47.21 | 38.36 | 1.23x (+23.1%) |
| Thread Histogram (minimal) | minimal | 45.83 | 38.36 | 1.19x (+19.5%) |
| Memory Trace (minimal) | minimal | 63.05 | 38.36 | 1.64x (+64.4%) |


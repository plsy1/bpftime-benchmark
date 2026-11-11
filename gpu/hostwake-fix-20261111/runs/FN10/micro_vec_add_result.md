
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:20:57.414060  

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
| Baseline (tiny) | tiny | 27.05 | - | - |
| Baseline (small) | small | 27.57 | - | - |
| Baseline (large) | large | 30.30 | - | - |
| Baseline (xlarge) | xlarge | 160.49 | - | - |
| Baseline (minimal) | minimal | 41.48 | - | - |
| Empty probe (tiny) | tiny | 36.39 | 27.05 | 1.35x (+34.5%) |
| Empty probe (xlarge) | xlarge | 171.04 | 160.49 | 1.07x (+6.6%) |
| Global timer (xlarge) | xlarge | 176.32 | 160.49 | 1.10x (+9.9%) |
| GPU Ringbuf (small) | small | 45.56 | 27.57 | 1.65x (+65.3%) |
| GPU Ringbuf (xlarge) | xlarge | 213.20 | 160.49 | 1.33x (+32.8%) |
| Per-GPU-thread array (small) | small | 39.00 | 27.57 | 1.41x (+41.5%) |
| Memtrace (small) | small | 30.35 | 27.57 | 1.10x (+10.1%) |
| Memtrace (large) | large | 174.30 | 30.30 | 5.75x (+475.2%) |
| GPU Array map update (small) | small | 34.15 | 27.57 | 1.24x (+23.9%) |
| GPU Array map update (large) | large | 239.64 | 30.30 | 7.91x (+690.9%) |
| GPU Array map lookup (small) | small | 34.02 | 27.57 | 1.23x (+23.4%) |
| GPU Array map lookup (large) | large | 64.86 | 30.30 | 2.14x (+114.1%) |
| CPU Array map update (minimal) | minimal | 51.10 | 27.05 | 1.89x (+88.9%) |
| CPU Array map lookup (minimal) | minimal | 50.98 | 27.05 | 1.88x (+88.5%) |
| CUDA Counter (minimal) | minimal | 2920.43 | 41.48 | 70.41x (+6940.6%) |
| Kernel Retsnoop (minimal) | minimal | 50.18 | 41.48 | 1.21x (+21.0%) |
| Thread Histogram (minimal) | minimal | 50.72 | 41.48 | 1.22x (+22.3%) |
| Memory Trace (minimal) | minimal | 53.10 | 41.48 | 1.28x (+28.0%) |



# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:06:15.212467  

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
| Baseline (tiny) | tiny | 25.39 | - | - |
| Baseline (small) | small | 25.91 | - | - |
| Baseline (large) | large | 31.14 | - | - |
| Baseline (xlarge) | xlarge | 160.56 | - | - |
| Baseline (minimal) | minimal | 38.90 | - | - |
| Empty probe (tiny) | tiny | 34.45 | 25.39 | 1.36x (+35.7%) |
| Empty probe (xlarge) | xlarge | 173.98 | 160.56 | 1.08x (+8.4%) |
| Global timer (xlarge) | xlarge | 175.21 | 160.56 | 1.09x (+9.1%) |
| GPU Ringbuf (small) | small | 42.39 | 25.91 | 1.64x (+63.6%) |
| GPU Ringbuf (xlarge) | xlarge | 218.47 | 160.56 | 1.36x (+36.1%) |
| Per-GPU-thread array (small) | small | 34.78 | 25.91 | 1.34x (+34.2%) |
| Memtrace (small) | small | 33.77 | 25.91 | 1.30x (+30.3%) |
| Memtrace (large) | large | 173.19 | 31.14 | 5.56x (+456.2%) |
| GPU Array map update (small) | small | 36.66 | 25.91 | 1.41x (+41.5%) |
| GPU Array map update (large) | large | 230.26 | 31.14 | 7.39x (+639.4%) |
| GPU Array map lookup (small) | small | 33.41 | 25.91 | 1.29x (+28.9%) |
| GPU Array map lookup (large) | large | 65.09 | 31.14 | 2.09x (+109.0%) |
| CPU Array map update (minimal) | minimal | 47.33 | 25.39 | 1.86x (+86.4%) |
| CPU Array map lookup (minimal) | minimal | 45.75 | 25.39 | 1.80x (+80.2%) |
| CUDA Counter (minimal) | minimal | 2942.27 | 38.90 | 75.64x (+7463.7%) |
| Kernel Retsnoop (minimal) | minimal | 51.76 | 38.90 | 1.33x (+33.1%) |
| Thread Histogram (minimal) | minimal | 50.80 | 38.90 | 1.31x (+30.6%) |
| Memory Trace (minimal) | minimal | 56.52 | 38.90 | 1.45x (+45.3%) |


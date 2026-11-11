
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:50:50.108084  

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
| Baseline (tiny) | tiny | 28.79 | - | - |
| Baseline (small) | small | 27.37 | - | - |
| Baseline (large) | large | 31.37 | - | - |
| Baseline (xlarge) | xlarge | 160.64 | - | - |
| Baseline (minimal) | minimal | 41.47 | - | - |
| Empty probe (tiny) | tiny | 29.93 | 28.79 | 1.04x (+4.0%) |
| Empty probe (xlarge) | xlarge | 172.86 | 160.64 | 1.08x (+7.6%) |
| Global timer (xlarge) | xlarge | 177.80 | 160.64 | 1.11x (+10.7%) |
| GPU Ringbuf (small) | small | 43.77 | 27.37 | 1.60x (+59.9%) |
| GPU Ringbuf (xlarge) | xlarge | 215.45 | 160.64 | 1.34x (+34.1%) |
| Per-GPU-thread array (small) | small | 36.45 | 27.37 | 1.33x (+33.2%) |
| Memtrace (small) | small | 35.78 | 27.37 | 1.31x (+30.7%) |
| Memtrace (large) | large | 178.41 | 31.37 | 5.69x (+468.7%) |
| GPU Array map update (small) | small | 36.56 | 27.37 | 1.34x (+33.6%) |
| GPU Array map update (large) | large | 229.83 | 31.37 | 7.33x (+632.6%) |
| GPU Array map lookup (small) | small | 36.22 | 27.37 | 1.32x (+32.3%) |
| GPU Array map lookup (large) | large | 66.13 | 31.37 | 2.11x (+110.8%) |
| CPU Array map update (minimal) | minimal | 38.12 | 28.79 | 1.32x (+32.4%) |
| CPU Array map lookup (minimal) | minimal | 41.37 | 28.79 | 1.44x (+43.7%) |
| CUDA Counter (minimal) | minimal | 2939.50 | 41.47 | 70.88x (+6988.3%) |
| Kernel Retsnoop (minimal) | minimal | 53.01 | 41.47 | 1.28x (+27.8%) |
| Thread Histogram (minimal) | minimal | 45.95 | 41.47 | 1.11x (+10.8%) |
| Memory Trace (minimal) | minimal | 62.03 | 41.47 | 1.50x (+49.6%) |


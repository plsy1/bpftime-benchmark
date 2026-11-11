
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:13:29.560697  

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
| Baseline (tiny) | tiny | 27.63 | - | - |
| Baseline (small) | small | 27.45 | - | - |
| Baseline (large) | large | 30.90 | - | - |
| Baseline (xlarge) | xlarge | 160.13 | - | - |
| Baseline (minimal) | minimal | 40.86 | - | - |
| Empty probe (tiny) | tiny | 34.23 | 27.63 | 1.24x (+23.9%) |
| Empty probe (xlarge) | xlarge | 171.43 | 160.13 | 1.07x (+7.1%) |
| Global timer (xlarge) | xlarge | 175.78 | 160.13 | 1.10x (+9.8%) |
| GPU Ringbuf (small) | small | 42.76 | 27.45 | 1.56x (+55.8%) |
| GPU Ringbuf (xlarge) | xlarge | 212.30 | 160.13 | 1.33x (+32.6%) |
| Per-GPU-thread array (small) | small | 37.92 | 27.45 | 1.38x (+38.1%) |
| Memtrace (small) | small | 31.05 | 27.45 | 1.13x (+13.1%) |
| Memtrace (large) | large | 175.95 | 30.90 | 5.69x (+469.4%) |
| GPU Array map update (small) | small | 33.94 | 27.45 | 1.24x (+23.6%) |
| GPU Array map update (large) | large | 246.51 | 30.90 | 7.98x (+697.8%) |
| GPU Array map lookup (small) | small | 37.03 | 27.45 | 1.35x (+34.9%) |
| GPU Array map lookup (large) | large | 64.77 | 30.90 | 2.10x (+109.6%) |
| CPU Array map update (minimal) | minimal | 49.25 | 27.63 | 1.78x (+78.2%) |
| CPU Array map lookup (minimal) | minimal | 48.32 | 27.63 | 1.75x (+74.9%) |
| CUDA Counter (minimal) | minimal | 2848.48 | 40.86 | 69.71x (+6871.3%) |
| Kernel Retsnoop (minimal) | minimal | 47.93 | 40.86 | 1.17x (+17.3%) |
| Thread Histogram (minimal) | minimal | 48.54 | 40.86 | 1.19x (+18.8%) |
| Memory Trace (minimal) | minimal | 57.85 | 40.86 | 1.42x (+41.6%) |


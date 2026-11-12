
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:28:28.955026  

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
| Baseline (tiny) | tiny | 25.76 | - | - |
| Baseline (small) | small | 26.93 | - | - |
| Baseline (large) | large | 31.18 | - | - |
| Baseline (xlarge) | xlarge | 158.89 | - | - |
| Baseline (minimal) | minimal | 38.39 | - | - |
| Empty probe (tiny) | tiny | 27.03 | 25.76 | 1.05x (+4.9%) |
| Empty probe (xlarge) | xlarge | 173.17 | 158.89 | 1.09x (+9.0%) |
| Global timer (xlarge) | xlarge | 179.87 | 158.89 | 1.13x (+13.2%) |
| GPU Ringbuf (small) | small | 45.32 | 26.93 | 1.68x (+68.3%) |
| GPU Ringbuf (xlarge) | xlarge | 218.20 | 158.89 | 1.37x (+37.3%) |
| Per-GPU-thread array (small) | small | 37.24 | 26.93 | 1.38x (+38.3%) |
| Memtrace (small) | small | 35.67 | 26.93 | 1.32x (+32.5%) |
| Memtrace (large) | large | 173.68 | 31.18 | 5.57x (+457.0%) |
| GPU Array map update (small) | small | 36.91 | 26.93 | 1.37x (+37.1%) |
| GPU Array map update (large) | large | 236.31 | 31.18 | 7.58x (+657.9%) |
| GPU Array map lookup (small) | small | 32.79 | 26.93 | 1.22x (+21.8%) |
| GPU Array map lookup (large) | large | 65.95 | 31.18 | 2.12x (+111.5%) |
| CPU Array map update (minimal) | minimal | 43.55 | 25.76 | 1.69x (+69.1%) |
| CPU Array map lookup (minimal) | minimal | 46.91 | 25.76 | 1.82x (+82.1%) |
| CUDA Counter (minimal) | minimal | 2984.24 | 38.39 | 77.73x (+7673.5%) |
| Kernel Retsnoop (minimal) | minimal | 48.18 | 38.39 | 1.26x (+25.5%) |
| Thread Histogram (minimal) | minimal | 42.40 | 38.39 | 1.10x (+10.4%) |
| Memory Trace (minimal) | minimal | 63.78 | 38.39 | 1.66x (+66.1%) |


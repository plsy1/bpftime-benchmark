
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:25:51.738819  

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
| Baseline (tiny) | tiny | 27.55 | - | - |
| Baseline (small) | small | 28.48 | - | - |
| Baseline (large) | large | 32.42 | - | - |
| Baseline (xlarge) | xlarge | 158.56 | - | - |
| Baseline (minimal) | minimal | 40.98 | - | - |
| Empty probe (tiny) | tiny | 34.70 | 27.55 | 1.26x (+26.0%) |
| Empty probe (xlarge) | xlarge | 172.24 | 158.56 | 1.09x (+8.6%) |
| Global timer (xlarge) | xlarge | 177.20 | 158.56 | 1.12x (+11.8%) |
| GPU Ringbuf (small) | small | 38.70 | 28.48 | 1.36x (+35.9%) |
| GPU Ringbuf (xlarge) | xlarge | 218.73 | 158.56 | 1.38x (+37.9%) |
| Per-GPU-thread array (small) | small | 35.43 | 28.48 | 1.24x (+24.4%) |
| Memtrace (small) | small | 38.19 | 28.48 | 1.34x (+34.1%) |
| Memtrace (large) | large | 169.78 | 32.42 | 5.24x (+423.7%) |
| GPU Array map update (small) | small | 40.62 | 28.48 | 1.43x (+42.6%) |
| GPU Array map update (large) | large | 241.64 | 32.42 | 7.45x (+645.3%) |
| GPU Array map lookup (small) | small | 34.50 | 28.48 | 1.21x (+21.1%) |
| GPU Array map lookup (large) | large | 65.99 | 32.42 | 2.04x (+103.5%) |
| CPU Array map update (minimal) | minimal | 50.12 | 27.55 | 1.82x (+81.9%) |
| CPU Array map lookup (minimal) | minimal | 49.22 | 27.55 | 1.79x (+78.7%) |
| CUDA Counter (minimal) | minimal | 2942.15 | 40.98 | 71.79x (+7079.5%) |
| Kernel Retsnoop (minimal) | minimal | 46.26 | 40.98 | 1.13x (+12.9%) |
| Thread Histogram (minimal) | minimal | 48.11 | 40.98 | 1.17x (+17.4%) |
| Memory Trace (minimal) | minimal | 54.33 | 40.98 | 1.33x (+32.6%) |



# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:20:02.911762  

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
| Baseline (tiny) | tiny | 27.39 | - | - |
| Baseline (small) | small | 27.69 | - | - |
| Baseline (large) | large | 31.15 | - | - |
| Baseline (xlarge) | xlarge | 159.90 | - | - |
| Baseline (minimal) | minimal | 39.68 | - | - |
| Empty probe (tiny) | tiny | 32.43 | 27.39 | 1.18x (+18.4%) |
| Empty probe (xlarge) | xlarge | 172.61 | 159.90 | 1.08x (+7.9%) |
| Global timer (xlarge) | xlarge | 174.75 | 159.90 | 1.09x (+9.3%) |
| GPU Ringbuf (small) | small | 41.65 | 27.69 | 1.50x (+50.4%) |
| GPU Ringbuf (xlarge) | xlarge | 211.79 | 159.90 | 1.32x (+32.5%) |
| Per-GPU-thread array (small) | small | 38.71 | 27.69 | 1.40x (+39.8%) |
| Memtrace (small) | small | 36.30 | 27.69 | 1.31x (+31.1%) |
| Memtrace (large) | large | 173.51 | 31.15 | 5.57x (+457.0%) |
| GPU Array map update (small) | small | 35.51 | 27.69 | 1.28x (+28.2%) |
| GPU Array map update (large) | large | 229.55 | 31.15 | 7.37x (+636.9%) |
| GPU Array map lookup (small) | small | 34.61 | 27.69 | 1.25x (+25.0%) |
| GPU Array map lookup (large) | large | 65.08 | 31.15 | 2.09x (+108.9%) |
| CPU Array map update (minimal) | minimal | 47.68 | 27.39 | 1.74x (+74.1%) |
| CPU Array map lookup (minimal) | minimal | 45.65 | 27.39 | 1.67x (+66.7%) |
| CUDA Counter (minimal) | minimal | 2916.16 | 39.68 | 73.49x (+7249.2%) |
| Kernel Retsnoop (minimal) | minimal | 56.41 | 39.68 | 1.42x (+42.2%) |
| Thread Histogram (minimal) | minimal | 42.91 | 39.68 | 1.08x (+8.1%) |
| Memory Trace (minimal) | minimal | 54.81 | 39.68 | 1.38x (+38.1%) |


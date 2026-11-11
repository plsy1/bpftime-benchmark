
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:46:06.076865  

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
| Baseline (tiny) | tiny | 27.97 | - | - |
| Baseline (small) | small | 27.13 | - | - |
| Baseline (large) | large | 30.97 | - | - |
| Baseline (xlarge) | xlarge | 159.62 | - | - |
| Baseline (minimal) | minimal | 43.80 | - | - |
| Empty probe (tiny) | tiny | 28.26 | 27.97 | 1.01x (+1.0%) |
| Empty probe (xlarge) | xlarge | 174.07 | 159.62 | 1.09x (+9.1%) |
| Global timer (xlarge) | xlarge | 181.86 | 159.62 | 1.14x (+13.9%) |
| GPU Ringbuf (small) | small | 45.43 | 27.13 | 1.67x (+67.5%) |
| GPU Ringbuf (xlarge) | xlarge | 217.75 | 159.62 | 1.36x (+36.4%) |
| Per-GPU-thread array (small) | small | 35.79 | 27.13 | 1.32x (+31.9%) |
| Memtrace (small) | small | 36.45 | 27.13 | 1.34x (+34.4%) |
| Memtrace (large) | large | 174.71 | 30.97 | 5.64x (+464.1%) |
| GPU Array map update (small) | small | 35.35 | 27.13 | 1.30x (+30.3%) |
| GPU Array map update (large) | large | 229.65 | 30.97 | 7.42x (+641.5%) |
| GPU Array map lookup (small) | small | 33.15 | 27.13 | 1.22x (+22.2%) |
| GPU Array map lookup (large) | large | 64.62 | 30.97 | 2.09x (+108.7%) |
| CPU Array map update (minimal) | minimal | 43.27 | 27.97 | 1.55x (+54.7%) |
| CPU Array map lookup (minimal) | minimal | 43.82 | 27.97 | 1.57x (+56.7%) |
| CUDA Counter (minimal) | minimal | 2923.98 | 43.80 | 66.76x (+6575.8%) |
| Kernel Retsnoop (minimal) | minimal | 47.64 | 43.80 | 1.09x (+8.8%) |
| Thread Histogram (minimal) | minimal | 40.95 | 43.80 | 0.93x (-6.5%) |
| Memory Trace (minimal) | minimal | 63.74 | 43.80 | 1.46x (+45.5%) |


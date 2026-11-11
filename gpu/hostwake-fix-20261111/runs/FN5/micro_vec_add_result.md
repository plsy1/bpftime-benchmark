
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:16:39.156282  

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
| Baseline (small) | small | 27.63 | - | - |
| Baseline (large) | large | 31.30 | - | - |
| Baseline (xlarge) | xlarge | 160.46 | - | - |
| Baseline (minimal) | minimal | 37.31 | - | - |
| Empty probe (tiny) | tiny | 30.33 | 27.63 | 1.10x (+9.8%) |
| Empty probe (xlarge) | xlarge | 172.28 | 160.46 | 1.07x (+7.4%) |
| Global timer (xlarge) | xlarge | 181.49 | 160.46 | 1.13x (+13.1%) |
| GPU Ringbuf (small) | small | 41.09 | 27.63 | 1.49x (+48.7%) |
| GPU Ringbuf (xlarge) | xlarge | 212.94 | 160.46 | 1.33x (+32.7%) |
| Per-GPU-thread array (small) | small | 38.67 | 27.63 | 1.40x (+40.0%) |
| Memtrace (small) | small | 36.98 | 27.63 | 1.34x (+33.8%) |
| Memtrace (large) | large | 170.03 | 31.30 | 5.43x (+443.2%) |
| GPU Array map update (small) | small | 34.86 | 27.63 | 1.26x (+26.2%) |
| GPU Array map update (large) | large | 244.80 | 31.30 | 7.82x (+682.1%) |
| GPU Array map lookup (small) | small | 32.70 | 27.63 | 1.18x (+18.3%) |
| GPU Array map lookup (large) | large | 66.71 | 31.30 | 2.13x (+113.1%) |
| CPU Array map update (minimal) | minimal | 47.70 | 27.63 | 1.73x (+72.6%) |
| CPU Array map lookup (minimal) | minimal | 46.82 | 27.63 | 1.69x (+69.5%) |
| CUDA Counter (minimal) | minimal | 2863.22 | 37.31 | 76.74x (+7574.1%) |
| Kernel Retsnoop (minimal) | minimal | 49.43 | 37.31 | 1.32x (+32.5%) |
| Thread Histogram (minimal) | minimal | 49.05 | 37.31 | 1.31x (+31.5%) |
| Memory Trace (minimal) | minimal | 57.44 | 37.31 | 1.54x (+54.0%) |


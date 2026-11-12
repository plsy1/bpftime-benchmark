
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:27:41.388541  

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
| Baseline (tiny) | tiny | 26.79 | - | - |
| Baseline (small) | small | 27.15 | - | - |
| Baseline (large) | large | 31.97 | - | - |
| Baseline (xlarge) | xlarge | 160.88 | - | - |
| Baseline (minimal) | minimal | 39.69 | - | - |
| Empty probe (tiny) | tiny | 29.08 | 26.79 | 1.09x (+8.5%) |
| Empty probe (xlarge) | xlarge | 175.66 | 160.88 | 1.09x (+9.2%) |
| Global timer (xlarge) | xlarge | 181.23 | 160.88 | 1.13x (+12.6%) |
| GPU Ringbuf (small) | small | 43.45 | 27.15 | 1.60x (+60.0%) |
| GPU Ringbuf (xlarge) | xlarge | 215.45 | 160.88 | 1.34x (+33.9%) |
| Per-GPU-thread array (small) | small | 38.92 | 27.15 | 1.43x (+43.4%) |
| Memtrace (small) | small | 38.13 | 27.15 | 1.40x (+40.4%) |
| Memtrace (large) | large | 177.11 | 31.97 | 5.54x (+454.0%) |
| GPU Array map update (small) | small | 36.71 | 27.15 | 1.35x (+35.2%) |
| GPU Array map update (large) | large | 229.26 | 31.97 | 7.17x (+617.1%) |
| GPU Array map lookup (small) | small | 34.34 | 27.15 | 1.26x (+26.5%) |
| GPU Array map lookup (large) | large | 66.94 | 31.97 | 2.09x (+109.4%) |
| CPU Array map update (minimal) | minimal | 47.01 | 26.79 | 1.75x (+75.5%) |
| CPU Array map lookup (minimal) | minimal | 48.52 | 26.79 | 1.81x (+81.1%) |
| CUDA Counter (minimal) | minimal | 2850.69 | 39.69 | 71.82x (+7082.4%) |
| Kernel Retsnoop (minimal) | minimal | 45.70 | 39.69 | 1.15x (+15.1%) |
| Thread Histogram (minimal) | minimal | 44.00 | 39.69 | 1.11x (+10.9%) |
| Memory Trace (minimal) | minimal | 57.64 | 39.69 | 1.45x (+45.2%) |


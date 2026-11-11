
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:20:10.214453  

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
| Baseline (tiny) | tiny | 27.14 | - | - |
| Baseline (small) | small | 27.18 | - | - |
| Baseline (large) | large | 31.25 | - | - |
| Baseline (xlarge) | xlarge | 158.63 | - | - |
| Baseline (minimal) | minimal | 39.23 | - | - |
| Empty probe (tiny) | tiny | 27.39 | 27.14 | 1.01x (+0.9%) |
| Empty probe (xlarge) | xlarge | 173.09 | 158.63 | 1.09x (+9.1%) |
| Global timer (xlarge) | xlarge | 176.31 | 158.63 | 1.11x (+11.1%) |
| GPU Ringbuf (small) | small | 41.37 | 27.18 | 1.52x (+52.2%) |
| GPU Ringbuf (xlarge) | xlarge | 218.95 | 158.63 | 1.38x (+38.0%) |
| Per-GPU-thread array (small) | small | 38.30 | 27.18 | 1.41x (+40.9%) |
| Memtrace (small) | small | 33.23 | 27.18 | 1.22x (+22.3%) |
| Memtrace (large) | large | 173.42 | 31.25 | 5.55x (+454.9%) |
| GPU Array map update (small) | small | 40.44 | 27.18 | 1.49x (+48.8%) |
| GPU Array map update (large) | large | 233.40 | 31.25 | 7.47x (+646.9%) |
| GPU Array map lookup (small) | small | 34.99 | 27.18 | 1.29x (+28.7%) |
| GPU Array map lookup (large) | large | 69.49 | 31.25 | 2.22x (+122.4%) |
| CPU Array map update (minimal) | minimal | 37.70 | 27.14 | 1.39x (+38.9%) |
| CPU Array map lookup (minimal) | minimal | 47.65 | 27.14 | 1.76x (+75.6%) |
| CUDA Counter (minimal) | minimal | 2895.14 | 39.23 | 73.80x (+7279.9%) |
| Kernel Retsnoop (minimal) | minimal | 45.23 | 39.23 | 1.15x (+15.3%) |
| Thread Histogram (minimal) | minimal | 50.87 | 39.23 | 1.30x (+29.7%) |
| Memory Trace (minimal) | minimal | 58.83 | 39.23 | 1.50x (+50.0%) |


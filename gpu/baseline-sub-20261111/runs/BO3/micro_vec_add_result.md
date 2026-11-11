
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:14:36.806211  

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
| Baseline (tiny) | tiny | 27.62 | - | - |
| Baseline (small) | small | 27.59 | - | - |
| Baseline (large) | large | 31.23 | - | - |
| Baseline (xlarge) | xlarge | 159.66 | - | - |
| Baseline (minimal) | minimal | 40.09 | - | - |
| Empty probe (tiny) | tiny | 30.18 | 27.62 | 1.09x (+9.3%) |
| Empty probe (xlarge) | xlarge | 176.62 | 159.66 | 1.11x (+10.6%) |
| Global timer (xlarge) | xlarge | 178.10 | 159.66 | 1.12x (+11.5%) |
| GPU Ringbuf (small) | small | 37.82 | 27.59 | 1.37x (+37.1%) |
| GPU Ringbuf (xlarge) | xlarge | 211.59 | 159.66 | 1.33x (+32.5%) |
| Per-GPU-thread array (small) | small | 39.44 | 27.59 | 1.43x (+43.0%) |
| Memtrace (small) | small | 40.13 | 27.59 | 1.45x (+45.5%) |
| Memtrace (large) | large | 175.26 | 31.23 | 5.61x (+461.2%) |
| GPU Array map update (small) | small | 38.75 | 27.59 | 1.40x (+40.4%) |
| GPU Array map update (large) | large | 226.66 | 31.23 | 7.26x (+625.8%) |
| GPU Array map lookup (small) | small | 31.62 | 27.59 | 1.15x (+14.6%) |
| GPU Array map lookup (large) | large | 69.43 | 31.23 | 2.22x (+122.3%) |
| CPU Array map update (minimal) | minimal | 33692.97 | 27.62 | 1219.88x (+121887.6%) |
| CPU Array map lookup (minimal) | minimal | 33752.36 | 27.62 | 1222.03x (+122102.6%) |
| CUDA Counter (minimal) | minimal | 337381.01 | 40.09 | 8415.59x (+841459.0%) |
| Kernel Retsnoop (minimal) | minimal | 62.11 | 40.09 | 1.55x (+54.9%) |
| Thread Histogram (minimal) | minimal | 50.29 | 40.09 | 1.25x (+25.4%) |
| Memory Trace (minimal) | minimal | 64.42 | 40.09 | 1.61x (+60.7%) |


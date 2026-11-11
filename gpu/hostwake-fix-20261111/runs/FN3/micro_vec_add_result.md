
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:15:06.073940  

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
| Baseline (tiny) | tiny | 27.05 | - | - |
| Baseline (small) | small | 28.16 | - | - |
| Baseline (large) | large | 31.81 | - | - |
| Baseline (xlarge) | xlarge | 161.04 | - | - |
| Baseline (minimal) | minimal | 41.31 | - | - |
| Empty probe (tiny) | tiny | 30.59 | 27.05 | 1.13x (+13.1%) |
| Empty probe (xlarge) | xlarge | 173.15 | 161.04 | 1.08x (+7.5%) |
| Global timer (xlarge) | xlarge | 175.43 | 161.04 | 1.09x (+8.9%) |
| GPU Ringbuf (small) | small | 45.27 | 28.16 | 1.61x (+60.8%) |
| GPU Ringbuf (xlarge) | xlarge | 213.72 | 161.04 | 1.33x (+32.7%) |
| Per-GPU-thread array (small) | small | 34.86 | 28.16 | 1.24x (+23.8%) |
| Memtrace (small) | small | 33.91 | 28.16 | 1.20x (+20.4%) |
| Memtrace (large) | large | 173.04 | 31.81 | 5.44x (+444.0%) |
| GPU Array map update (small) | small | 37.96 | 28.16 | 1.35x (+34.8%) |
| GPU Array map update (large) | large | 239.19 | 31.81 | 7.52x (+651.9%) |
| GPU Array map lookup (small) | small | 34.78 | 28.16 | 1.24x (+23.5%) |
| GPU Array map lookup (large) | large | 66.40 | 31.81 | 2.09x (+108.7%) |
| CPU Array map update (minimal) | minimal | 47.42 | 27.05 | 1.75x (+75.3%) |
| CPU Array map lookup (minimal) | minimal | 46.84 | 27.05 | 1.73x (+73.2%) |
| CUDA Counter (minimal) | minimal | 2852.99 | 41.31 | 69.06x (+6806.3%) |
| Kernel Retsnoop (minimal) | minimal | 51.88 | 41.31 | 1.26x (+25.6%) |
| Thread Histogram (minimal) | minimal | 46.15 | 41.31 | 1.12x (+11.7%) |
| Memory Trace (minimal) | minimal | 56.44 | 41.31 | 1.37x (+36.6%) |


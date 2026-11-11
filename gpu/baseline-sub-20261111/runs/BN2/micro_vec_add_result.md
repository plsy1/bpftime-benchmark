
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:12:38.922281  

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
| Baseline (tiny) | tiny | 27.36 | - | - |
| Baseline (small) | small | 27.38 | - | - |
| Baseline (large) | large | 31.00 | - | - |
| Baseline (xlarge) | xlarge | 160.73 | - | - |
| Baseline (minimal) | minimal | 39.28 | - | - |
| Empty probe (tiny) | tiny | 32.68 | 27.36 | 1.19x (+19.4%) |
| Empty probe (xlarge) | xlarge | 174.58 | 160.73 | 1.09x (+8.6%) |
| Global timer (xlarge) | xlarge | 176.31 | 160.73 | 1.10x (+9.7%) |
| GPU Ringbuf (small) | small | 42.26 | 27.38 | 1.54x (+54.3%) |
| GPU Ringbuf (xlarge) | xlarge | 213.70 | 160.73 | 1.33x (+33.0%) |
| Per-GPU-thread array (small) | small | 33.56 | 27.38 | 1.23x (+22.6%) |
| Memtrace (small) | small | 37.15 | 27.38 | 1.36x (+35.7%) |
| Memtrace (large) | large | 171.06 | 31.00 | 5.52x (+451.8%) |
| GPU Array map update (small) | small | 36.22 | 27.38 | 1.32x (+32.3%) |
| GPU Array map update (large) | large | 246.83 | 31.00 | 7.96x (+696.2%) |
| GPU Array map lookup (small) | small | 33.14 | 27.38 | 1.21x (+21.0%) |
| GPU Array map lookup (large) | large | 66.32 | 31.00 | 2.14x (+113.9%) |
| CPU Array map update (minimal) | minimal | 62.19 | 27.36 | 2.27x (+127.3%) |
| CPU Array map lookup (minimal) | minimal | 55.08 | 27.36 | 2.01x (+101.3%) |
| CUDA Counter (minimal) | minimal | 2936.12 | 39.28 | 74.75x (+7374.8%) |
| Kernel Retsnoop (minimal) | minimal | 52.08 | 39.28 | 1.33x (+32.6%) |
| Thread Histogram (minimal) | minimal | 51.68 | 39.28 | 1.32x (+31.6%) |
| Memory Trace (minimal) | minimal | 61.86 | 39.28 | 1.57x (+57.5%) |


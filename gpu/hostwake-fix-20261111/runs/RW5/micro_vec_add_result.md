
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:47:17.095111  

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
| Baseline (tiny) | tiny | 27.74 | - | - |
| Baseline (small) | small | 28.47 | - | - |
| Baseline (large) | large | 31.04 | - | - |
| Baseline (xlarge) | xlarge | 159.44 | - | - |
| Baseline (minimal) | minimal | 41.11 | - | - |
| Empty probe (tiny) | tiny | 29.60 | 27.74 | 1.07x (+6.7%) |
| Empty probe (xlarge) | xlarge | 174.14 | 159.44 | 1.09x (+9.2%) |
| Global timer (xlarge) | xlarge | 178.23 | 159.44 | 1.12x (+11.8%) |
| GPU Ringbuf (small) | small | 44.59 | 28.47 | 1.57x (+56.6%) |
| GPU Ringbuf (xlarge) | xlarge | 217.40 | 159.44 | 1.36x (+36.4%) |
| Per-GPU-thread array (small) | small | 37.56 | 28.47 | 1.32x (+31.9%) |
| Memtrace (small) | small | 36.67 | 28.47 | 1.29x (+28.8%) |
| Memtrace (large) | large | 176.96 | 31.04 | 5.70x (+470.1%) |
| GPU Array map update (small) | small | 37.74 | 28.47 | 1.33x (+32.6%) |
| GPU Array map update (large) | large | 229.39 | 31.04 | 7.39x (+639.0%) |
| GPU Array map lookup (small) | small | 34.07 | 28.47 | 1.20x (+19.7%) |
| GPU Array map lookup (large) | large | 66.39 | 31.04 | 2.14x (+113.9%) |
| CPU Array map update (minimal) | minimal | 39.82 | 27.74 | 1.44x (+43.5%) |
| CPU Array map lookup (minimal) | minimal | 43.79 | 27.74 | 1.58x (+57.9%) |
| CUDA Counter (minimal) | minimal | 2963.04 | 41.11 | 72.08x (+7107.6%) |
| Kernel Retsnoop (minimal) | minimal | 46.11 | 41.11 | 1.12x (+12.2%) |
| Thread Histogram (minimal) | minimal | 49.77 | 41.11 | 1.21x (+21.1%) |
| Memory Trace (minimal) | minimal | 63.99 | 41.11 | 1.56x (+55.7%) |


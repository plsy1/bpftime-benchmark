
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:18:29.792468  

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
| Baseline (tiny) | tiny | 28.50 | - | - |
| Baseline (small) | small | 26.15 | - | - |
| Baseline (large) | large | 31.42 | - | - |
| Baseline (xlarge) | xlarge | 159.86 | - | - |
| Baseline (minimal) | minimal | 37.57 | - | - |
| Empty probe (tiny) | tiny | 34.99 | 28.50 | 1.23x (+22.8%) |
| Empty probe (xlarge) | xlarge | 168.82 | 159.86 | 1.06x (+5.6%) |
| Global timer (xlarge) | xlarge | 175.47 | 159.86 | 1.10x (+9.8%) |
| GPU Ringbuf (small) | small | 40.96 | 26.15 | 1.57x (+56.6%) |
| GPU Ringbuf (xlarge) | xlarge | 215.94 | 159.86 | 1.35x (+35.1%) |
| Per-GPU-thread array (small) | small | 37.83 | 26.15 | 1.45x (+44.7%) |
| Memtrace (small) | small | 36.80 | 26.15 | 1.41x (+40.7%) |
| Memtrace (large) | large | 170.52 | 31.42 | 5.43x (+442.7%) |
| GPU Array map update (small) | small | 33.55 | 26.15 | 1.28x (+28.3%) |
| GPU Array map update (large) | large | 240.11 | 31.42 | 7.64x (+664.2%) |
| GPU Array map lookup (small) | small | 33.04 | 26.15 | 1.26x (+26.3%) |
| GPU Array map lookup (large) | large | 68.39 | 31.42 | 2.18x (+117.7%) |
| CPU Array map update (minimal) | minimal | 52.99 | 28.50 | 1.86x (+85.9%) |
| CPU Array map lookup (minimal) | minimal | 51.90 | 28.50 | 1.82x (+82.1%) |
| CUDA Counter (minimal) | minimal | 2913.66 | 37.57 | 77.55x (+7655.3%) |
| Kernel Retsnoop (minimal) | minimal | 48.35 | 37.57 | 1.29x (+28.7%) |
| Thread Histogram (minimal) | minimal | 49.54 | 37.57 | 1.32x (+31.9%) |
| Memory Trace (minimal) | minimal | 57.67 | 37.57 | 1.54x (+53.5%) |



# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:17:02.428142  

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
| Baseline (tiny) | tiny | 27.20 | - | - |
| Baseline (small) | small | 27.08 | - | - |
| Baseline (large) | large | 31.45 | - | - |
| Baseline (xlarge) | xlarge | 161.49 | - | - |
| Baseline (minimal) | minimal | 40.70 | - | - |
| Empty probe (tiny) | tiny | 26.98 | 27.20 | 0.99x (-0.8%) |
| Empty probe (xlarge) | xlarge | 173.48 | 161.49 | 1.07x (+7.4%) |
| Global timer (xlarge) | xlarge | 178.89 | 161.49 | 1.11x (+10.8%) |
| GPU Ringbuf (small) | small | 47.33 | 27.08 | 1.75x (+74.8%) |
| GPU Ringbuf (xlarge) | xlarge | 214.29 | 161.49 | 1.33x (+32.7%) |
| Per-GPU-thread array (small) | small | 37.70 | 27.08 | 1.39x (+39.2%) |
| Memtrace (small) | small | 33.99 | 27.08 | 1.26x (+25.5%) |
| Memtrace (large) | large | 180.70 | 31.45 | 5.75x (+474.6%) |
| GPU Array map update (small) | small | 38.16 | 27.08 | 1.41x (+40.9%) |
| GPU Array map update (large) | large | 226.58 | 31.45 | 7.20x (+620.4%) |
| GPU Array map lookup (small) | small | 34.76 | 27.08 | 1.28x (+28.4%) |
| GPU Array map lookup (large) | large | 68.40 | 31.45 | 2.17x (+117.5%) |
| CPU Array map update (minimal) | minimal | 50.32 | 27.20 | 1.85x (+85.0%) |
| CPU Array map lookup (minimal) | minimal | 46.05 | 27.20 | 1.69x (+69.3%) |
| CUDA Counter (minimal) | minimal | 2906.78 | 40.70 | 71.42x (+7042.0%) |
| Kernel Retsnoop (minimal) | minimal | 53.05 | 40.70 | 1.30x (+30.3%) |
| Thread Histogram (minimal) | minimal | 52.32 | 40.70 | 1.29x (+28.6%) |
| Memory Trace (minimal) | minimal | 61.99 | 40.70 | 1.52x (+52.3%) |



# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:18:59.749946  

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
| Baseline (tiny) | tiny | 27.30 | - | - |
| Baseline (small) | small | 26.03 | - | - |
| Baseline (large) | large | 30.60 | - | - |
| Baseline (xlarge) | xlarge | 159.96 | - | - |
| Baseline (minimal) | minimal | 40.07 | - | - |
| Empty probe (tiny) | tiny | 30.24 | 27.30 | 1.11x (+10.8%) |
| Empty probe (xlarge) | xlarge | 173.87 | 159.96 | 1.09x (+8.7%) |
| Global timer (xlarge) | xlarge | 181.82 | 159.96 | 1.14x (+13.7%) |
| GPU Ringbuf (small) | small | 46.08 | 26.03 | 1.77x (+77.0%) |
| GPU Ringbuf (xlarge) | xlarge | 217.40 | 159.96 | 1.36x (+35.9%) |
| Per-GPU-thread array (small) | small | 37.08 | 26.03 | 1.42x (+42.5%) |
| Memtrace (small) | small | 36.29 | 26.03 | 1.39x (+39.4%) |
| Memtrace (large) | large | 176.02 | 30.60 | 5.75x (+475.2%) |
| GPU Array map update (small) | small | 39.59 | 26.03 | 1.52x (+52.1%) |
| GPU Array map update (large) | large | 220.45 | 30.60 | 7.20x (+620.4%) |
| GPU Array map lookup (small) | small | 36.32 | 26.03 | 1.40x (+39.5%) |
| GPU Array map lookup (large) | large | 66.17 | 30.60 | 2.16x (+116.2%) |
| CPU Array map update (minimal) | minimal | 38.87 | 27.30 | 1.42x (+42.4%) |
| CPU Array map lookup (minimal) | minimal | 44.75 | 27.30 | 1.64x (+63.9%) |
| CUDA Counter (minimal) | minimal | 2882.63 | 40.07 | 71.94x (+7094.0%) |
| Kernel Retsnoop (minimal) | minimal | 49.84 | 40.07 | 1.24x (+24.4%) |
| Thread Histogram (minimal) | minimal | 53.73 | 40.07 | 1.34x (+34.1%) |
| Memory Trace (minimal) | minimal | 61.76 | 40.07 | 1.54x (+54.1%) |


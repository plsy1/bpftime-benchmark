
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:44:54.357343  

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
| Baseline (tiny) | tiny | 26.97 | - | - |
| Baseline (small) | small | 27.21 | - | - |
| Baseline (large) | large | 31.40 | - | - |
| Baseline (xlarge) | xlarge | 160.06 | - | - |
| Baseline (minimal) | minimal | 38.99 | - | - |
| Empty probe (tiny) | tiny | 34.98 | 26.97 | 1.30x (+29.7%) |
| Empty probe (xlarge) | xlarge | 170.57 | 160.06 | 1.07x (+6.6%) |
| Global timer (xlarge) | xlarge | 185.51 | 160.06 | 1.16x (+15.9%) |
| GPU Ringbuf (small) | small | 41.93 | 27.21 | 1.54x (+54.1%) |
| GPU Ringbuf (xlarge) | xlarge | 220.46 | 160.06 | 1.38x (+37.7%) |
| Per-GPU-thread array (small) | small | 35.12 | 27.21 | 1.29x (+29.1%) |
| Memtrace (small) | small | 35.08 | 27.21 | 1.29x (+28.9%) |
| Memtrace (large) | large | 177.38 | 31.40 | 5.65x (+464.9%) |
| GPU Array map update (small) | small | 34.76 | 27.21 | 1.28x (+27.7%) |
| GPU Array map update (large) | large | 239.39 | 31.40 | 7.62x (+662.4%) |
| GPU Array map lookup (small) | small | 34.20 | 27.21 | 1.26x (+25.7%) |
| GPU Array map lookup (large) | large | 67.41 | 31.40 | 2.15x (+114.7%) |
| CPU Array map update (minimal) | minimal | 51.54 | 26.97 | 1.91x (+91.1%) |
| CPU Array map lookup (minimal) | minimal | 50.58 | 26.97 | 1.88x (+87.5%) |
| CUDA Counter (minimal) | minimal | 3035.12 | 38.99 | 77.84x (+7684.4%) |
| Kernel Retsnoop (minimal) | minimal | 46.92 | 38.99 | 1.20x (+20.3%) |
| Thread Histogram (minimal) | minimal | 50.42 | 38.99 | 1.29x (+29.3%) |
| Memory Trace (minimal) | minimal | 60.79 | 38.99 | 1.56x (+55.9%) |


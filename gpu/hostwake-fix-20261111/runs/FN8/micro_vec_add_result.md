
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:19:23.390582  

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
| Baseline (tiny) | tiny | 26.90 | - | - |
| Baseline (small) | small | 27.32 | - | - |
| Baseline (large) | large | 31.48 | - | - |
| Baseline (xlarge) | xlarge | 160.50 | - | - |
| Baseline (minimal) | minimal | 41.62 | - | - |
| Empty probe (tiny) | tiny | 35.00 | 26.90 | 1.30x (+30.1%) |
| Empty probe (xlarge) | xlarge | 174.80 | 160.50 | 1.09x (+8.9%) |
| Global timer (xlarge) | xlarge | 177.55 | 160.50 | 1.11x (+10.6%) |
| GPU Ringbuf (small) | small | 39.84 | 27.32 | 1.46x (+45.8%) |
| GPU Ringbuf (xlarge) | xlarge | 213.90 | 160.50 | 1.33x (+33.3%) |
| Per-GPU-thread array (small) | small | 32.81 | 27.32 | 1.20x (+20.1%) |
| Memtrace (small) | small | 36.48 | 27.32 | 1.34x (+33.5%) |
| Memtrace (large) | large | 175.02 | 31.48 | 5.56x (+456.0%) |
| GPU Array map update (small) | small | 33.68 | 27.32 | 1.23x (+23.3%) |
| GPU Array map update (large) | large | 239.08 | 31.48 | 7.59x (+659.5%) |
| GPU Array map lookup (small) | small | 34.05 | 27.32 | 1.25x (+24.6%) |
| GPU Array map lookup (large) | large | 65.05 | 31.48 | 2.07x (+106.6%) |
| CPU Array map update (minimal) | minimal | 57.74 | 26.90 | 2.15x (+114.6%) |
| CPU Array map lookup (minimal) | minimal | 47.19 | 26.90 | 1.75x (+75.4%) |
| CUDA Counter (minimal) | minimal | 2970.11 | 41.62 | 71.36x (+7036.3%) |
| Kernel Retsnoop (minimal) | minimal | 47.58 | 41.62 | 1.14x (+14.3%) |
| Thread Histogram (minimal) | minimal | 44.59 | 41.62 | 1.07x (+7.1%) |
| Memory Trace (minimal) | minimal | 58.08 | 41.62 | 1.40x (+39.5%) |


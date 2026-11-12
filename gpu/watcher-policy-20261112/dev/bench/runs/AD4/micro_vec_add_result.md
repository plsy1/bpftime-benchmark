
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:23:43.536196  

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
| Baseline (tiny) | tiny | 27.59 | - | - |
| Baseline (small) | small | 27.41 | - | - |
| Baseline (large) | large | 31.11 | - | - |
| Baseline (xlarge) | xlarge | 159.76 | - | - |
| Baseline (minimal) | minimal | 38.37 | - | - |
| Empty probe (tiny) | tiny | 29.21 | 27.59 | 1.06x (+5.9%) |
| Empty probe (xlarge) | xlarge | 175.92 | 159.76 | 1.10x (+10.1%) |
| Global timer (xlarge) | xlarge | 179.89 | 159.76 | 1.13x (+12.6%) |
| GPU Ringbuf (small) | small | 42.94 | 27.41 | 1.57x (+56.7%) |
| GPU Ringbuf (xlarge) | xlarge | 213.89 | 159.76 | 1.34x (+33.9%) |
| Per-GPU-thread array (small) | small | 36.89 | 27.41 | 1.35x (+34.6%) |
| Memtrace (small) | small | 31.67 | 27.41 | 1.16x (+15.5%) |
| Memtrace (large) | large | 178.26 | 31.11 | 5.73x (+473.0%) |
| GPU Array map update (small) | small | 35.04 | 27.41 | 1.28x (+27.8%) |
| GPU Array map update (large) | large | 231.14 | 31.11 | 7.43x (+643.0%) |
| GPU Array map lookup (small) | small | 32.69 | 27.41 | 1.19x (+19.3%) |
| GPU Array map lookup (large) | large | 69.44 | 31.11 | 2.23x (+123.2%) |
| CPU Array map update (minimal) | minimal | 47.24 | 27.59 | 1.71x (+71.2%) |
| CPU Array map lookup (minimal) | minimal | 52.36 | 27.59 | 1.90x (+89.8%) |
| CUDA Counter (minimal) | minimal | 2987.33 | 38.37 | 77.86x (+7685.6%) |
| Kernel Retsnoop (minimal) | minimal | 48.53 | 38.37 | 1.26x (+26.5%) |
| Thread Histogram (minimal) | minimal | 44.59 | 38.37 | 1.16x (+16.2%) |
| Memory Trace (minimal) | minimal | 66.74 | 38.37 | 1.74x (+73.9%) |


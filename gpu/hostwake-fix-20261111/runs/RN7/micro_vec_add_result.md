
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:48:28.027885  

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
| Baseline (tiny) | tiny | 26.31 | - | - |
| Baseline (small) | small | 27.39 | - | - |
| Baseline (large) | large | 31.62 | - | - |
| Baseline (xlarge) | xlarge | 163.35 | - | - |
| Baseline (minimal) | minimal | 35.80 | - | - |
| Empty probe (tiny) | tiny | 35.63 | 26.31 | 1.35x (+35.4%) |
| Empty probe (xlarge) | xlarge | 171.76 | 163.35 | 1.05x (+5.1%) |
| Global timer (xlarge) | xlarge | 178.71 | 163.35 | 1.09x (+9.4%) |
| GPU Ringbuf (small) | small | 43.89 | 27.39 | 1.60x (+60.2%) |
| GPU Ringbuf (xlarge) | xlarge | 214.46 | 163.35 | 1.31x (+31.3%) |
| Per-GPU-thread array (small) | small | 36.99 | 27.39 | 1.35x (+35.0%) |
| Memtrace (small) | small | 38.06 | 27.39 | 1.39x (+39.0%) |
| Memtrace (large) | large | 170.29 | 31.62 | 5.39x (+438.6%) |
| GPU Array map update (small) | small | 39.62 | 27.39 | 1.45x (+44.7%) |
| GPU Array map update (large) | large | 248.43 | 31.62 | 7.86x (+685.7%) |
| GPU Array map lookup (small) | small | 33.28 | 27.39 | 1.22x (+21.5%) |
| GPU Array map lookup (large) | large | 65.49 | 31.62 | 2.07x (+107.1%) |
| CPU Array map update (minimal) | minimal | 44.07 | 26.31 | 1.68x (+67.5%) |
| CPU Array map lookup (minimal) | minimal | 49.73 | 26.31 | 1.89x (+89.0%) |
| CUDA Counter (minimal) | minimal | 2873.31 | 35.80 | 80.26x (+7926.0%) |
| Kernel Retsnoop (minimal) | minimal | 49.20 | 35.80 | 1.37x (+37.4%) |
| Thread Histogram (minimal) | minimal | 51.27 | 35.80 | 1.43x (+43.2%) |
| Memory Trace (minimal) | minimal | 56.54 | 35.80 | 1.58x (+57.9%) |


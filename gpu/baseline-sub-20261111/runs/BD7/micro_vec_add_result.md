
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:23:09.576525  

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
| Baseline (tiny) | tiny | 27.75 | - | - |
| Baseline (small) | small | 27.53 | - | - |
| Baseline (large) | large | 31.07 | - | - |
| Baseline (xlarge) | xlarge | 159.49 | - | - |
| Baseline (minimal) | minimal | 39.21 | - | - |
| Empty probe (tiny) | tiny | 33.21 | 27.75 | 1.20x (+19.7%) |
| Empty probe (xlarge) | xlarge | 175.03 | 159.49 | 1.10x (+9.7%) |
| Global timer (xlarge) | xlarge | 174.77 | 159.49 | 1.10x (+9.6%) |
| GPU Ringbuf (small) | small | 39.97 | 27.53 | 1.45x (+45.2%) |
| GPU Ringbuf (xlarge) | xlarge | 214.18 | 159.49 | 1.34x (+34.3%) |
| Per-GPU-thread array (small) | small | 36.45 | 27.53 | 1.32x (+32.4%) |
| Memtrace (small) | small | 35.95 | 27.53 | 1.31x (+30.6%) |
| Memtrace (large) | large | 174.17 | 31.07 | 5.61x (+460.6%) |
| GPU Array map update (small) | small | 39.58 | 27.53 | 1.44x (+43.8%) |
| GPU Array map update (large) | large | 245.49 | 31.07 | 7.90x (+690.1%) |
| GPU Array map lookup (small) | small | 33.57 | 27.53 | 1.22x (+21.9%) |
| GPU Array map lookup (large) | large | 68.19 | 31.07 | 2.19x (+119.5%) |
| CPU Array map update (minimal) | minimal | 335.91 | 27.75 | 12.10x (+1110.5%) |
| CPU Array map lookup (minimal) | minimal | 377.39 | 27.75 | 13.60x (+1260.0%) |
| CUDA Counter (minimal) | minimal | 3557.19 | 39.21 | 90.72x (+8972.1%) |
| Kernel Retsnoop (minimal) | minimal | 46.05 | 39.21 | 1.17x (+17.4%) |
| Thread Histogram (minimal) | minimal | 45.24 | 39.21 | 1.15x (+15.4%) |
| Memory Trace (minimal) | minimal | 54.07 | 39.21 | 1.38x (+37.9%) |


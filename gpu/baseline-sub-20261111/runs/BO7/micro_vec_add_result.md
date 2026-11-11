
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:22:00.211323  

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
| Baseline (tiny) | tiny | 27.24 | - | - |
| Baseline (small) | small | 27.98 | - | - |
| Baseline (large) | large | 31.15 | - | - |
| Baseline (xlarge) | xlarge | 158.81 | - | - |
| Baseline (minimal) | minimal | 37.35 | - | - |
| Empty probe (tiny) | tiny | 31.63 | 27.24 | 1.16x (+16.1%) |
| Empty probe (xlarge) | xlarge | 178.73 | 158.81 | 1.13x (+12.5%) |
| Global timer (xlarge) | xlarge | 179.99 | 158.81 | 1.13x (+13.3%) |
| GPU Ringbuf (small) | small | 39.26 | 27.98 | 1.40x (+40.3%) |
| GPU Ringbuf (xlarge) | xlarge | 213.89 | 158.81 | 1.35x (+34.7%) |
| Per-GPU-thread array (small) | small | 40.56 | 27.98 | 1.45x (+45.0%) |
| Memtrace (small) | small | 38.39 | 27.98 | 1.37x (+37.2%) |
| Memtrace (large) | large | 175.68 | 31.15 | 5.64x (+464.0%) |
| GPU Array map update (small) | small | 40.03 | 27.98 | 1.43x (+43.1%) |
| GPU Array map update (large) | large | 223.34 | 31.15 | 7.17x (+617.0%) |
| GPU Array map lookup (small) | small | 37.91 | 27.98 | 1.35x (+35.5%) |
| GPU Array map lookup (large) | large | 66.51 | 31.15 | 2.14x (+113.5%) |
| CPU Array map update (minimal) | minimal | 33744.36 | 27.24 | 1238.78x (+123778.0%) |
| CPU Array map lookup (minimal) | minimal | 33730.11 | 27.24 | 1238.26x (+123725.7%) |
| CUDA Counter (minimal) | minimal | 337808.67 | 37.35 | 9044.41x (+904340.9%) |
| Kernel Retsnoop (minimal) | minimal | 59.29 | 37.35 | 1.59x (+58.7%) |
| Thread Histogram (minimal) | minimal | 42.32 | 37.35 | 1.13x (+13.3%) |
| Memory Trace (minimal) | minimal | 64.46 | 37.35 | 1.73x (+72.6%) |


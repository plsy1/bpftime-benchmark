
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:24:54.090665  

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
| Baseline (tiny) | tiny | 26.72 | - | - |
| Baseline (small) | small | 27.51 | - | - |
| Baseline (large) | large | 31.43 | - | - |
| Baseline (xlarge) | xlarge | 159.80 | - | - |
| Baseline (minimal) | minimal | 41.67 | - | - |
| Empty probe (tiny) | tiny | 32.11 | 26.72 | 1.20x (+20.2%) |
| Empty probe (xlarge) | xlarge | 175.47 | 159.80 | 1.10x (+9.8%) |
| Global timer (xlarge) | xlarge | 178.60 | 159.80 | 1.12x (+11.8%) |
| GPU Ringbuf (small) | small | 49.13 | 27.51 | 1.79x (+78.6%) |
| GPU Ringbuf (xlarge) | xlarge | 218.61 | 159.80 | 1.37x (+36.8%) |
| Per-GPU-thread array (small) | small | 36.18 | 27.51 | 1.32x (+31.5%) |
| Memtrace (small) | small | 35.90 | 27.51 | 1.30x (+30.5%) |
| Memtrace (large) | large | 174.97 | 31.43 | 5.57x (+456.7%) |
| GPU Array map update (small) | small | 39.01 | 27.51 | 1.42x (+41.8%) |
| GPU Array map update (large) | large | 239.03 | 31.43 | 7.61x (+660.5%) |
| GPU Array map lookup (small) | small | 37.54 | 27.51 | 1.36x (+36.5%) |
| GPU Array map lookup (large) | large | 68.10 | 31.43 | 2.17x (+116.7%) |
| CPU Array map update (minimal) | minimal | 38.08 | 26.72 | 1.43x (+42.5%) |
| CPU Array map lookup (minimal) | minimal | 49.29 | 26.72 | 1.84x (+84.5%) |
| CUDA Counter (minimal) | minimal | 2948.97 | 41.67 | 70.77x (+6977.0%) |
| Kernel Retsnoop (minimal) | minimal | 45.26 | 41.67 | 1.09x (+8.6%) |
| Thread Histogram (minimal) | minimal | 40.75 | 41.67 | 0.97x (-2.2%) |
| Memory Trace (minimal) | minimal | 60.60 | 41.67 | 1.45x (+45.4%) |



# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:26:53.809098  

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
| Baseline (tiny) | tiny | 27.02 | - | - |
| Baseline (small) | small | 27.45 | - | - |
| Baseline (large) | large | 31.07 | - | - |
| Baseline (xlarge) | xlarge | 159.81 | - | - |
| Baseline (minimal) | minimal | 39.23 | - | - |
| Empty probe (tiny) | tiny | 29.54 | 27.02 | 1.09x (+9.3%) |
| Empty probe (xlarge) | xlarge | 177.60 | 159.81 | 1.11x (+11.1%) |
| Global timer (xlarge) | xlarge | 177.97 | 159.81 | 1.11x (+11.4%) |
| GPU Ringbuf (small) | small | 45.44 | 27.45 | 1.66x (+65.5%) |
| GPU Ringbuf (xlarge) | xlarge | 212.95 | 159.81 | 1.33x (+33.3%) |
| Per-GPU-thread array (small) | small | 36.76 | 27.45 | 1.34x (+33.9%) |
| Memtrace (small) | small | 34.37 | 27.45 | 1.25x (+25.2%) |
| Memtrace (large) | large | 173.44 | 31.07 | 5.58x (+458.2%) |
| GPU Array map update (small) | small | 33.04 | 27.45 | 1.20x (+20.4%) |
| GPU Array map update (large) | large | 234.56 | 31.07 | 7.55x (+654.9%) |
| GPU Array map lookup (small) | small | 32.13 | 27.45 | 1.17x (+17.0%) |
| GPU Array map lookup (large) | large | 67.82 | 31.07 | 2.18x (+118.3%) |
| CPU Array map update (minimal) | minimal | 41.27 | 27.02 | 1.53x (+52.7%) |
| CPU Array map lookup (minimal) | minimal | 46.84 | 27.02 | 1.73x (+73.4%) |
| CUDA Counter (minimal) | minimal | 2979.80 | 39.23 | 75.96x (+7495.7%) |
| Kernel Retsnoop (minimal) | minimal | 49.02 | 39.23 | 1.25x (+25.0%) |
| Thread Histogram (minimal) | minimal | 42.31 | 39.23 | 1.08x (+7.9%) |
| Memory Trace (minimal) | minimal | 62.79 | 39.23 | 1.60x (+60.1%) |


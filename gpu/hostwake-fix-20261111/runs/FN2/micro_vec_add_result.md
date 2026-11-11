
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:14:42.233615  

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
| Baseline (tiny) | tiny | 27.12 | - | - |
| Baseline (small) | small | 27.06 | - | - |
| Baseline (large) | large | 31.32 | - | - |
| Baseline (xlarge) | xlarge | 160.84 | - | - |
| Baseline (minimal) | minimal | 39.87 | - | - |
| Empty probe (tiny) | tiny | 32.96 | 27.12 | 1.22x (+21.5%) |
| Empty probe (xlarge) | xlarge | 175.42 | 160.84 | 1.09x (+9.1%) |
| Global timer (xlarge) | xlarge | 181.38 | 160.84 | 1.13x (+12.8%) |
| GPU Ringbuf (small) | small | 43.36 | 27.06 | 1.60x (+60.2%) |
| GPU Ringbuf (xlarge) | xlarge | 213.12 | 160.84 | 1.33x (+32.5%) |
| Per-GPU-thread array (small) | small | 37.50 | 27.06 | 1.39x (+38.6%) |
| Memtrace (small) | small | 32.65 | 27.06 | 1.21x (+20.7%) |
| Memtrace (large) | large | 171.17 | 31.32 | 5.47x (+446.5%) |
| GPU Array map update (small) | small | 38.69 | 27.06 | 1.43x (+43.0%) |
| GPU Array map update (large) | large | 235.08 | 31.32 | 7.51x (+650.6%) |
| GPU Array map lookup (small) | small | 31.86 | 27.06 | 1.18x (+17.7%) |
| GPU Array map lookup (large) | large | 64.06 | 31.32 | 2.05x (+104.5%) |
| CPU Array map update (minimal) | minimal | 50.98 | 27.12 | 1.88x (+88.0%) |
| CPU Array map lookup (minimal) | minimal | 53.37 | 27.12 | 1.97x (+96.8%) |
| CUDA Counter (minimal) | minimal | 2877.26 | 39.87 | 72.17x (+7116.6%) |
| Kernel Retsnoop (minimal) | minimal | 48.83 | 39.87 | 1.22x (+22.5%) |
| Thread Histogram (minimal) | minimal | 44.99 | 39.87 | 1.13x (+12.8%) |
| Memory Trace (minimal) | minimal | 57.60 | 39.87 | 1.44x (+44.5%) |


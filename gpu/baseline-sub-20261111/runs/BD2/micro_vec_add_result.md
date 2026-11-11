
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:12:15.210680  

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
| Baseline (tiny) | tiny | 25.89 | - | - |
| Baseline (small) | small | 27.97 | - | - |
| Baseline (large) | large | 31.57 | - | - |
| Baseline (xlarge) | xlarge | 158.48 | - | - |
| Baseline (minimal) | minimal | 39.21 | - | - |
| Empty probe (tiny) | tiny | 34.22 | 25.89 | 1.32x (+32.2%) |
| Empty probe (xlarge) | xlarge | 172.10 | 158.48 | 1.09x (+8.6%) |
| Global timer (xlarge) | xlarge | 176.19 | 158.48 | 1.11x (+11.2%) |
| GPU Ringbuf (small) | small | 41.69 | 27.97 | 1.49x (+49.1%) |
| GPU Ringbuf (xlarge) | xlarge | 214.30 | 158.48 | 1.35x (+35.2%) |
| Per-GPU-thread array (small) | small | 32.98 | 27.97 | 1.18x (+17.9%) |
| Memtrace (small) | small | 32.16 | 27.97 | 1.15x (+15.0%) |
| Memtrace (large) | large | 176.24 | 31.57 | 5.58x (+458.3%) |
| GPU Array map update (small) | small | 37.54 | 27.97 | 1.34x (+34.2%) |
| GPU Array map update (large) | large | 245.23 | 31.57 | 7.77x (+676.8%) |
| GPU Array map lookup (small) | small | 34.64 | 27.97 | 1.24x (+23.8%) |
| GPU Array map lookup (large) | large | 67.81 | 31.57 | 2.15x (+114.8%) |
| CPU Array map update (minimal) | minimal | 319.65 | 25.89 | 12.35x (+1134.6%) |
| CPU Array map lookup (minimal) | minimal | 374.07 | 25.89 | 14.45x (+1344.8%) |
| CUDA Counter (minimal) | minimal | 3468.83 | 39.21 | 88.47x (+8746.8%) |
| Kernel Retsnoop (minimal) | minimal | 49.54 | 39.21 | 1.26x (+26.3%) |
| Thread Histogram (minimal) | minimal | 48.63 | 39.21 | 1.24x (+24.0%) |
| Memory Trace (minimal) | minimal | 60.60 | 39.21 | 1.55x (+54.6%) |


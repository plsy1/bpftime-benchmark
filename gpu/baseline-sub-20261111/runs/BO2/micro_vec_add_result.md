
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:13:02.917719  

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
| Baseline (tiny) | tiny | 25.73 | - | - |
| Baseline (small) | small | 27.38 | - | - |
| Baseline (large) | large | 31.09 | - | - |
| Baseline (xlarge) | xlarge | 159.72 | - | - |
| Baseline (minimal) | minimal | 39.40 | - | - |
| Empty probe (tiny) | tiny | 34.44 | 25.73 | 1.34x (+33.9%) |
| Empty probe (xlarge) | xlarge | 176.10 | 159.72 | 1.10x (+10.3%) |
| Global timer (xlarge) | xlarge | 176.19 | 159.72 | 1.10x (+10.3%) |
| GPU Ringbuf (small) | small | 39.16 | 27.38 | 1.43x (+43.0%) |
| GPU Ringbuf (xlarge) | xlarge | 208.42 | 159.72 | 1.30x (+30.5%) |
| Per-GPU-thread array (small) | small | 38.53 | 27.38 | 1.41x (+40.7%) |
| Memtrace (small) | small | 36.61 | 27.38 | 1.34x (+33.7%) |
| Memtrace (large) | large | 181.50 | 31.09 | 5.84x (+483.8%) |
| GPU Array map update (small) | small | 40.78 | 27.38 | 1.49x (+48.9%) |
| GPU Array map update (large) | large | 220.39 | 31.09 | 7.09x (+608.9%) |
| GPU Array map lookup (small) | small | 35.39 | 27.38 | 1.29x (+29.3%) |
| GPU Array map lookup (large) | large | 66.67 | 31.09 | 2.14x (+114.4%) |
| CPU Array map update (minimal) | minimal | 33729.00 | 25.73 | 1310.88x (+130988.2%) |
| CPU Array map lookup (minimal) | minimal | 33625.81 | 25.73 | 1306.87x (+130587.2%) |
| CUDA Counter (minimal) | minimal | 337399.25 | 39.40 | 8563.43x (+856243.3%) |
| Kernel Retsnoop (minimal) | minimal | 58.00 | 39.40 | 1.47x (+47.2%) |
| Thread Histogram (minimal) | minimal | 41.26 | 39.40 | 1.05x (+4.7%) |
| Memory Trace (minimal) | minimal | 64.59 | 39.40 | 1.64x (+63.9%) |


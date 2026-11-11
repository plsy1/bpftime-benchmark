
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:16:10.122629  

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
| Baseline (tiny) | tiny | 26.32 | - | - |
| Baseline (small) | small | 26.91 | - | - |
| Baseline (large) | large | 31.48 | - | - |
| Baseline (xlarge) | xlarge | 159.48 | - | - |
| Baseline (minimal) | minimal | 38.85 | - | - |
| Empty probe (tiny) | tiny | 34.06 | 26.32 | 1.29x (+29.4%) |
| Empty probe (xlarge) | xlarge | 175.19 | 159.48 | 1.10x (+9.9%) |
| Global timer (xlarge) | xlarge | 176.37 | 159.48 | 1.11x (+10.6%) |
| GPU Ringbuf (small) | small | 39.85 | 26.91 | 1.48x (+48.1%) |
| GPU Ringbuf (xlarge) | xlarge | 210.85 | 159.48 | 1.32x (+32.2%) |
| Per-GPU-thread array (small) | small | 39.59 | 26.91 | 1.47x (+47.1%) |
| Memtrace (small) | small | 37.99 | 26.91 | 1.41x (+41.2%) |
| Memtrace (large) | large | 181.95 | 31.48 | 5.78x (+478.0%) |
| GPU Array map update (small) | small | 41.49 | 26.91 | 1.54x (+54.2%) |
| GPU Array map update (large) | large | 218.17 | 31.48 | 6.93x (+593.0%) |
| GPU Array map lookup (small) | small | 37.11 | 26.91 | 1.38x (+37.9%) |
| GPU Array map lookup (large) | large | 67.44 | 31.48 | 2.14x (+114.2%) |
| CPU Array map update (minimal) | minimal | 33695.32 | 26.32 | 1280.22x (+127921.7%) |
| CPU Array map lookup (minimal) | minimal | 33768.05 | 26.32 | 1282.98x (+128198.1%) |
| CUDA Counter (minimal) | minimal | 337450.31 | 38.85 | 8685.98x (+868498.0%) |
| Kernel Retsnoop (minimal) | minimal | 58.53 | 38.85 | 1.51x (+50.7%) |
| Thread Histogram (minimal) | minimal | 59.43 | 38.85 | 1.53x (+53.0%) |
| Memory Trace (minimal) | minimal | 63.55 | 38.85 | 1.64x (+63.6%) |


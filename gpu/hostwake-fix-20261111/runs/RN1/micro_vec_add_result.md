
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:43:41.540021  

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
| Baseline (tiny) | tiny | 27.41 | - | - |
| Baseline (small) | small | 27.41 | - | - |
| Baseline (large) | large | 30.97 | - | - |
| Baseline (xlarge) | xlarge | 160.77 | - | - |
| Baseline (minimal) | minimal | 42.23 | - | - |
| Empty probe (tiny) | tiny | 30.71 | 27.41 | 1.12x (+12.0%) |
| Empty probe (xlarge) | xlarge | 172.63 | 160.77 | 1.07x (+7.4%) |
| Global timer (xlarge) | xlarge | 181.08 | 160.77 | 1.13x (+12.6%) |
| GPU Ringbuf (small) | small | 43.06 | 27.41 | 1.57x (+57.1%) |
| GPU Ringbuf (xlarge) | xlarge | 213.71 | 160.77 | 1.33x (+32.9%) |
| Per-GPU-thread array (small) | small | 38.56 | 27.41 | 1.41x (+40.7%) |
| Memtrace (small) | small | 35.46 | 27.41 | 1.29x (+29.4%) |
| Memtrace (large) | large | 170.14 | 30.97 | 5.49x (+449.4%) |
| GPU Array map update (small) | small | 38.37 | 27.41 | 1.40x (+40.0%) |
| GPU Array map update (large) | large | 247.44 | 30.97 | 7.99x (+699.0%) |
| GPU Array map lookup (small) | small | 38.94 | 27.41 | 1.42x (+42.1%) |
| GPU Array map lookup (large) | large | 72.00 | 30.97 | 2.32x (+132.5%) |
| CPU Array map update (minimal) | minimal | 49.18 | 27.41 | 1.79x (+79.4%) |
| CPU Array map lookup (minimal) | minimal | 54.63 | 27.41 | 1.99x (+99.3%) |
| CUDA Counter (minimal) | minimal | 2867.67 | 42.23 | 67.91x (+6690.6%) |
| Kernel Retsnoop (minimal) | minimal | 55.35 | 42.23 | 1.31x (+31.1%) |
| Thread Histogram (minimal) | minimal | 52.71 | 42.23 | 1.25x (+24.8%) |
| Memory Trace (minimal) | minimal | 60.10 | 42.23 | 1.42x (+42.3%) |



# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:49:15.329006  

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
| Baseline (tiny) | tiny | 26.93 | - | - |
| Baseline (small) | small | 27.41 | - | - |
| Baseline (large) | large | 31.32 | - | - |
| Baseline (xlarge) | xlarge | 158.63 | - | - |
| Baseline (minimal) | minimal | 36.25 | - | - |
| Empty probe (tiny) | tiny | 27.16 | 26.93 | 1.01x (+0.9%) |
| Empty probe (xlarge) | xlarge | 173.98 | 158.63 | 1.10x (+9.7%) |
| Global timer (xlarge) | xlarge | 182.28 | 158.63 | 1.15x (+14.9%) |
| GPU Ringbuf (small) | small | 46.41 | 27.41 | 1.69x (+69.3%) |
| GPU Ringbuf (xlarge) | xlarge | 214.93 | 158.63 | 1.35x (+35.5%) |
| Per-GPU-thread array (small) | small | 37.27 | 27.41 | 1.36x (+36.0%) |
| Memtrace (small) | small | 37.72 | 27.41 | 1.38x (+37.6%) |
| Memtrace (large) | large | 178.38 | 31.32 | 5.70x (+469.5%) |
| GPU Array map update (small) | small | 36.38 | 27.41 | 1.33x (+32.7%) |
| GPU Array map update (large) | large | 231.45 | 31.32 | 7.39x (+639.0%) |
| GPU Array map lookup (small) | small | 34.26 | 27.41 | 1.25x (+25.0%) |
| GPU Array map lookup (large) | large | 69.51 | 31.32 | 2.22x (+121.9%) |
| CPU Array map update (minimal) | minimal | 39.61 | 26.93 | 1.47x (+47.1%) |
| CPU Array map lookup (minimal) | minimal | 40.05 | 26.93 | 1.49x (+48.7%) |
| CUDA Counter (minimal) | minimal | 2838.33 | 36.25 | 78.30x (+7729.9%) |
| Kernel Retsnoop (minimal) | minimal | 50.32 | 36.25 | 1.39x (+38.8%) |
| Thread Histogram (minimal) | minimal | 33.20 | 36.25 | 0.91x (-8.4%) |
| Memory Trace (minimal) | minimal | 62.38 | 36.25 | 1.72x (+72.1%) |


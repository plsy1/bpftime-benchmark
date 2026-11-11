
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:02:32.659079  

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
| Baseline (tiny) | tiny | 28.08 | - | - |
| Baseline (small) | small | 27.62 | - | - |
| Baseline (large) | large | 32.56 | - | - |
| Baseline (xlarge) | xlarge | 157.89 | - | - |
| Baseline (minimal) | minimal | 40.95 | - | - |
| Empty probe (tiny) | tiny | 35.48 | 28.08 | 1.26x (+26.4%) |
| Empty probe (xlarge) | xlarge | 174.59 | 157.89 | 1.11x (+10.6%) |
| Global timer (xlarge) | xlarge | 177.80 | 157.89 | 1.13x (+12.6%) |
| GPU Ringbuf (small) | small | 47.60 | 27.62 | 1.72x (+72.3%) |
| GPU Ringbuf (xlarge) | xlarge | 214.07 | 157.89 | 1.36x (+35.6%) |
| Per-GPU-thread array (small) | small | 38.18 | 27.62 | 1.38x (+38.2%) |
| Memtrace (small) | small | 29.88 | 27.62 | 1.08x (+8.2%) |
| Memtrace (large) | large | 170.84 | 32.56 | 5.25x (+424.7%) |
| GPU Array map update (small) | small | 36.30 | 27.62 | 1.31x (+31.4%) |
| GPU Array map update (large) | large | 245.46 | 32.56 | 7.54x (+653.9%) |
| GPU Array map lookup (small) | small | 38.83 | 27.62 | 1.41x (+40.6%) |
| GPU Array map lookup (large) | large | 68.07 | 32.56 | 2.09x (+109.1%) |
| CPU Array map update (minimal) | minimal | 327.04 | 28.08 | 11.65x (+1064.7%) |
| CPU Array map lookup (minimal) | minimal | 374.77 | 28.08 | 13.35x (+1234.7%) |
| CUDA Counter (minimal) | minimal | 3557.19 | 40.95 | 86.87x (+8586.7%) |
| Kernel Retsnoop (minimal) | minimal | 49.34 | 40.95 | 1.20x (+20.5%) |
| Thread Histogram (minimal) | minimal | 51.31 | 40.95 | 1.25x (+25.3%) |
| Memory Trace (minimal) | minimal | 61.09 | 40.95 | 1.49x (+49.2%) |


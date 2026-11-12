
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:26:29.958233  

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
| Baseline (tiny) | tiny | 27.25 | - | - |
| Baseline (small) | small | 26.91 | - | - |
| Baseline (large) | large | 30.41 | - | - |
| Baseline (xlarge) | xlarge | 159.15 | - | - |
| Baseline (minimal) | minimal | 38.21 | - | - |
| Empty probe (tiny) | tiny | 31.52 | 27.25 | 1.16x (+15.7%) |
| Empty probe (xlarge) | xlarge | 176.86 | 159.15 | 1.11x (+11.1%) |
| Global timer (xlarge) | xlarge | 175.34 | 159.15 | 1.10x (+10.2%) |
| GPU Ringbuf (small) | small | 43.20 | 26.91 | 1.61x (+60.5%) |
| GPU Ringbuf (xlarge) | xlarge | 216.37 | 159.15 | 1.36x (+36.0%) |
| Per-GPU-thread array (small) | small | 37.95 | 26.91 | 1.41x (+41.0%) |
| Memtrace (small) | small | 36.50 | 26.91 | 1.36x (+35.6%) |
| Memtrace (large) | large | 175.96 | 30.41 | 5.79x (+478.6%) |
| GPU Array map update (small) | small | 37.94 | 26.91 | 1.41x (+41.0%) |
| GPU Array map update (large) | large | 229.26 | 30.41 | 7.54x (+653.9%) |
| GPU Array map lookup (small) | small | 35.31 | 26.91 | 1.31x (+31.2%) |
| GPU Array map lookup (large) | large | 68.29 | 30.41 | 2.25x (+124.6%) |
| CPU Array map update (minimal) | minimal | 48.50 | 27.25 | 1.78x (+78.0%) |
| CPU Array map lookup (minimal) | minimal | 48.70 | 27.25 | 1.79x (+78.7%) |
| CUDA Counter (minimal) | minimal | 2876.14 | 38.21 | 75.27x (+7427.2%) |
| Kernel Retsnoop (minimal) | minimal | 52.25 | 38.21 | 1.37x (+36.7%) |
| Thread Histogram (minimal) | minimal | 43.77 | 38.21 | 1.15x (+14.6%) |
| Memory Trace (minimal) | minimal | 61.54 | 38.21 | 1.61x (+61.1%) |


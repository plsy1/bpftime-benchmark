
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:48:51.797899  

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
| Baseline (tiny) | tiny | 28.14 | - | - |
| Baseline (small) | small | 27.30 | - | - |
| Baseline (large) | large | 31.26 | - | - |
| Baseline (xlarge) | xlarge | 158.65 | - | - |
| Baseline (minimal) | minimal | 40.43 | - | - |
| Empty probe (tiny) | tiny | 28.40 | 28.14 | 1.01x (+0.9%) |
| Empty probe (xlarge) | xlarge | 173.55 | 158.65 | 1.09x (+9.4%) |
| Global timer (xlarge) | xlarge | 177.03 | 158.65 | 1.12x (+11.6%) |
| GPU Ringbuf (small) | small | 47.75 | 27.30 | 1.75x (+74.9%) |
| GPU Ringbuf (xlarge) | xlarge | 215.14 | 158.65 | 1.36x (+35.6%) |
| Per-GPU-thread array (small) | small | 36.60 | 27.30 | 1.34x (+34.1%) |
| Memtrace (small) | small | 37.00 | 27.30 | 1.36x (+35.5%) |
| Memtrace (large) | large | 178.70 | 31.26 | 5.72x (+471.7%) |
| GPU Array map update (small) | small | 38.04 | 27.30 | 1.39x (+39.3%) |
| GPU Array map update (large) | large | 230.50 | 31.26 | 7.37x (+637.4%) |
| GPU Array map lookup (small) | small | 34.39 | 27.30 | 1.26x (+26.0%) |
| GPU Array map lookup (large) | large | 70.96 | 31.26 | 2.27x (+127.0%) |
| CPU Array map update (minimal) | minimal | 45.22 | 28.14 | 1.61x (+60.7%) |
| CPU Array map lookup (minimal) | minimal | 37.33 | 28.14 | 1.33x (+32.7%) |
| CUDA Counter (minimal) | minimal | 2944.52 | 40.43 | 72.83x (+7183.0%) |
| Kernel Retsnoop (minimal) | minimal | 49.03 | 40.43 | 1.21x (+21.3%) |
| Thread Histogram (minimal) | minimal | 50.33 | 40.43 | 1.24x (+24.5%) |
| Memory Trace (minimal) | minimal | 60.06 | 40.43 | 1.49x (+48.6%) |


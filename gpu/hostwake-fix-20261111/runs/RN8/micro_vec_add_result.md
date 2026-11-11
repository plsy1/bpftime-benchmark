
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:49:39.035689  

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
| Baseline (tiny) | tiny | 26.72 | - | - |
| Baseline (small) | small | 27.66 | - | - |
| Baseline (large) | large | 31.00 | - | - |
| Baseline (xlarge) | xlarge | 159.79 | - | - |
| Baseline (minimal) | minimal | 40.42 | - | - |
| Empty probe (tiny) | tiny | 35.49 | 26.72 | 1.33x (+32.8%) |
| Empty probe (xlarge) | xlarge | 171.25 | 159.79 | 1.07x (+7.2%) |
| Global timer (xlarge) | xlarge | 175.60 | 159.79 | 1.10x (+9.9%) |
| GPU Ringbuf (small) | small | 47.35 | 27.66 | 1.71x (+71.2%) |
| GPU Ringbuf (xlarge) | xlarge | 210.24 | 159.79 | 1.32x (+31.6%) |
| Per-GPU-thread array (small) | small | 32.13 | 27.66 | 1.16x (+16.2%) |
| Memtrace (small) | small | 37.79 | 27.66 | 1.37x (+36.6%) |
| Memtrace (large) | large | 173.35 | 31.00 | 5.59x (+459.2%) |
| GPU Array map update (small) | small | 35.98 | 27.66 | 1.30x (+30.1%) |
| GPU Array map update (large) | large | 226.12 | 31.00 | 7.29x (+629.4%) |
| GPU Array map lookup (small) | small | 36.25 | 27.66 | 1.31x (+31.1%) |
| GPU Array map lookup (large) | large | 71.47 | 31.00 | 2.31x (+130.5%) |
| CPU Array map update (minimal) | minimal | 51.97 | 26.72 | 1.94x (+94.5%) |
| CPU Array map lookup (minimal) | minimal | 53.19 | 26.72 | 1.99x (+99.1%) |
| CUDA Counter (minimal) | minimal | 2905.22 | 40.42 | 71.88x (+7087.6%) |
| Kernel Retsnoop (minimal) | minimal | 46.90 | 40.42 | 1.16x (+16.0%) |
| Thread Histogram (minimal) | minimal | 48.67 | 40.42 | 1.20x (+20.4%) |
| Memory Trace (minimal) | minimal | 61.14 | 40.42 | 1.51x (+51.3%) |


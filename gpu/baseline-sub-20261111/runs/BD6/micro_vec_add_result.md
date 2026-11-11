
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:21:36.348366  

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
| Baseline (tiny) | tiny | 27.29 | - | - |
| Baseline (small) | small | 28.40 | - | - |
| Baseline (large) | large | 32.81 | - | - |
| Baseline (xlarge) | xlarge | 160.38 | - | - |
| Baseline (minimal) | minimal | 35.82 | - | - |
| Empty probe (tiny) | tiny | 35.17 | 27.29 | 1.29x (+28.9%) |
| Empty probe (xlarge) | xlarge | 171.41 | 160.38 | 1.07x (+6.9%) |
| Global timer (xlarge) | xlarge | 175.03 | 160.38 | 1.09x (+9.1%) |
| GPU Ringbuf (small) | small | 42.87 | 28.40 | 1.51x (+51.0%) |
| GPU Ringbuf (xlarge) | xlarge | 216.23 | 160.38 | 1.35x (+34.8%) |
| Per-GPU-thread array (small) | small | 37.90 | 28.40 | 1.33x (+33.5%) |
| Memtrace (small) | small | 39.81 | 28.40 | 1.40x (+40.2%) |
| Memtrace (large) | large | 173.63 | 32.81 | 5.29x (+429.2%) |
| GPU Array map update (small) | small | 35.03 | 28.40 | 1.23x (+23.3%) |
| GPU Array map update (large) | large | 243.35 | 32.81 | 7.42x (+641.7%) |
| GPU Array map lookup (small) | small | 34.47 | 28.40 | 1.21x (+21.4%) |
| GPU Array map lookup (large) | large | 67.03 | 32.81 | 2.04x (+104.3%) |
| CPU Array map update (minimal) | minimal | 325.06 | 27.29 | 11.91x (+1091.1%) |
| CPU Array map lookup (minimal) | minimal | 365.60 | 27.29 | 13.40x (+1239.7%) |
| CUDA Counter (minimal) | minimal | 3537.76 | 35.82 | 98.76x (+9776.5%) |
| Kernel Retsnoop (minimal) | minimal | 48.36 | 35.82 | 1.35x (+35.0%) |
| Thread Histogram (minimal) | minimal | 46.70 | 35.82 | 1.30x (+30.4%) |
| Memory Trace (minimal) | minimal | 54.66 | 35.82 | 1.53x (+52.6%) |


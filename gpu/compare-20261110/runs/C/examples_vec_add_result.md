
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T12:17:43.361888  

## Workload Configuration

| Workload | Binary | Elements | Iterations | Threads | Blocks |
|----------|--------|----------|------------|---------|--------|
| large | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add | 100000 | 1000 | 512 | 196 |
| medium | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add | 10000 | 10000 | 256 | 40 |
| minimal | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add | 32 | 3 | 32 | 1 |
| small | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add | 1000 | 10000 | 256 | 4 |
| tiny | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add | 32 | 10000 | 32 | 1 |
| xlarge | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add | 1000000 | 1000 | 512 | 1954 |

## Benchmark Results

| Test Name | Workload | Avg Time (μs) | vs Baseline | Overhead |
|-----------|----------|---------------|-------------|----------|
| Baseline (minimal) | minimal | 37.86 | - | - |
| CUDA Counter (minimal) | minimal | 3471.02 | 37.86 | 91.68x (+9068.0%) |
| Memory Trace (minimal) | minimal | 57.26 | 37.86 | 1.51x (+51.2%) |
| Thread Histogram (minimal) | minimal | 45.78 | 37.86 | 1.21x (+20.9%) |
| Kernel Retsnoop (minimal) | minimal | 50.04 | 37.86 | 1.32x (+32.2%) |


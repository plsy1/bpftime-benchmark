
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T15:11:57.838106  

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
| Baseline (minimal) | minimal | 39.60 | - | - |
| S both (minimal) | minimal | 51.24 | 39.60 | 1.29x (+29.4%) |
| S memtrace (minimal) | minimal | 49.02 | 39.60 | 1.24x (+23.8%) |
| S pergputd-array-lookup (minimal) | minimal | 56.10 | 39.60 | 1.42x (+41.7%) |
| S gpu-array-update (minimal) | minimal | 59.90 | 39.60 | 1.51x (+51.3%) |
| S ringbuf (minimal) | minimal | 59.94 | 39.60 | 1.51x (+51.4%) |
| S empty (minimal) | minimal | 45.52 | 39.60 | 1.15x (+14.9%) |
| S Kernel Retsnoop (minimal) | minimal | 61.09 | 39.60 | 1.54x (+54.3%) |


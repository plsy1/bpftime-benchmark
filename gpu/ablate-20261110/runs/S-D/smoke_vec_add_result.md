
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T15:08:08.000350  

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
| Baseline (minimal) | minimal | 37.92 | - | - |
| S both (minimal) | minimal | 48.83 | 37.92 | 1.29x (+28.8%) |
| S memtrace (minimal) | minimal | 55.82 | 37.92 | 1.47x (+47.2%) |
| S pergputd-array-lookup (minimal) | minimal | 55.82 | 37.92 | 1.47x (+47.2%) |
| S gpu-array-update (minimal) | minimal | 61.15 | 37.92 | 1.61x (+61.3%) |
| S ringbuf (minimal) | minimal | 56.86 | 37.92 | 1.50x (+49.9%) |
| S empty (minimal) | minimal | 54.73 | 37.92 | 1.44x (+44.3%) |
| S Kernel Retsnoop (minimal) | minimal | 59.95 | 37.92 | 1.58x (+58.1%) |


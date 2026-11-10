
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T15:13:14.312363  

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
| Baseline (minimal) | minimal | 41.14 | - | - |
| S both (minimal) | minimal | 50.63 | 41.14 | 1.23x (+23.1%) |
| S memtrace (minimal) | minimal | 54.59 | 41.14 | 1.33x (+32.7%) |
| S pergputd-array-lookup (minimal) | minimal | 66.08 | 41.14 | 1.61x (+60.6%) |
| S gpu-array-update (minimal) | minimal | 62.07 | 41.14 | 1.51x (+50.9%) |
| S ringbuf (minimal) | minimal | 65.10 | 41.14 | 1.58x (+58.2%) |
| S empty (minimal) | minimal | 41.23 | 41.14 | 1.00x (+0.2%) |
| S Kernel Retsnoop (minimal) | minimal | 65.35 | 41.14 | 1.59x (+58.8%) |


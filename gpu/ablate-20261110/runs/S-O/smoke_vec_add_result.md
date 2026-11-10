
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T15:06:33.807230  

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
| Baseline (minimal) | minimal | 35.94 | - | - |
| S both (minimal) | minimal | 36.81 | 35.94 | 1.02x (+2.4%) |
| S memtrace (minimal) | minimal | 47.71 | 35.94 | 1.33x (+32.7%) |
| S pergputd-array-lookup (minimal) | minimal | 67.19 | 35.94 | 1.87x (+87.0%) |
| S gpu-array-update (minimal) | minimal | 67.34 | 35.94 | 1.87x (+87.4%) |
| S ringbuf (minimal) | minimal | 80.82 | 35.94 | 2.25x (+124.9%) |
| S empty (minimal) | minimal | 51.88 | 35.94 | 1.44x (+44.4%) |
| S Kernel Retsnoop (minimal) | minimal | 73.54 | 35.94 | 2.05x (+104.6%) |


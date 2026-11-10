
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T15:09:24.549472  

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
| Baseline (minimal) | minimal | 38.31 | - | - |
| S both (minimal) | minimal | 44.88 | 38.31 | 1.17x (+17.1%) |
| S memtrace (minimal) | minimal | 56.30 | 38.31 | 1.47x (+47.0%) |
| S pergputd-array-lookup (minimal) | minimal | 68.47 | 38.31 | 1.79x (+78.7%) |
| S gpu-array-update (minimal) | minimal | 72.14 | 38.31 | 1.88x (+88.3%) |
| S ringbuf (minimal) | minimal | 74.49 | 38.31 | 1.94x (+94.4%) |
| S empty (minimal) | minimal | 41.05 | 38.31 | 1.07x (+7.2%) |
| S Kernel Retsnoop (minimal) | minimal | 69.23 | 38.31 | 1.81x (+80.7%) |



# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T15:10:41.006390  

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
| Baseline (minimal) | minimal | 41.06 | - | - |
| S both (minimal) | minimal | 50.38 | 41.06 | 1.23x (+22.7%) |
| S memtrace (minimal) | minimal | 65.26 | 41.06 | 1.59x (+58.9%) |
| S pergputd-array-lookup (minimal) | minimal | 63.15 | 41.06 | 1.54x (+53.8%) |
| S gpu-array-update (minimal) | minimal | 61.79 | 41.06 | 1.50x (+50.5%) |
| S ringbuf (minimal) | minimal | 63.86 | 41.06 | 1.56x (+55.5%) |
| S empty (minimal) | minimal | 45.96 | 41.06 | 1.12x (+11.9%) |
| S Kernel Retsnoop (minimal) | minimal | 64.07 | 41.06 | 1.56x (+56.0%) |


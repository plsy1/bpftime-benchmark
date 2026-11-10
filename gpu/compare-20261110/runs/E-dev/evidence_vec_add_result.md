
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T10:53:45.542886  

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
| Baseline (minimal) | minimal | 40.36 | - | - |
| E empty (minimal) | minimal | 48.44 | 40.36 | 1.20x (+20.0%) |
| E entry (minimal) | minimal | 45.62 | 40.36 | 1.13x (+13.0%) |
| E exit (minimal) | minimal | 45.08 | 40.36 | 1.12x (+11.7%) |
| E both (minimal) | minimal | 53.73 | 40.36 | 1.33x (+33.1%) |
| E ringbuf (minimal) | minimal | 68.22 | 40.36 | 1.69x (+69.0%) |
| E globaltimer (minimal) | minimal | 54.88 | 40.36 | 1.36x (+36.0%) |
| E pergputd-array-lookup (minimal) | minimal | 61.61 | 40.36 | 1.53x (+52.7%) |
| E memtrace (minimal) | minimal | 57.73 | 40.36 | 1.43x (+43.0%) |
| E gpu-array-update (minimal) | minimal | 57.59 | 40.36 | 1.43x (+42.7%) |
| E gpu-array-lookup (minimal) | minimal | 55.52 | 40.36 | 1.38x (+37.6%) |
| E array-update (minimal) | minimal | 463.11 | 40.36 | 11.47x (+1047.4%) |
| E array-lookup (minimal) | minimal | 496.49 | 40.36 | 12.30x (+1130.2%) |
| E hash-update (minimal) | minimal | 458.62 | 40.36 | 11.36x (+1036.3%) |
| E hash-lookup (minimal) | minimal | 475.86 | 40.36 | 11.79x (+1079.0%) |
| E hash-delete (minimal) | minimal | 423.84 | 40.36 | 10.50x (+950.1%) |
| E CUDA Counter (minimal) | minimal | 4734.71 | 40.36 | 117.31x (+11631.2%) |
| E Kernel Retsnoop (minimal) | minimal | 58.98 | 40.36 | 1.46x (+46.1%) |


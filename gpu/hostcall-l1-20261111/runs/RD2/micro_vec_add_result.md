
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:31:14.373263  

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
| Baseline (tiny) | tiny | 27.61 | - | - |
| Baseline (small) | small | 27.40 | - | - |
| Baseline (minimal) | minimal | 38.31 | - | - |
| CPU Array map update (minimal) | minimal | 324.61 | 27.61 | 11.76x (+1075.7%) |
| CPU Array map lookup (minimal) | minimal | 377.32 | 27.61 | 13.67x (+1266.6%) |
| CPU Hash map update (minimal) | minimal | 325.05 | 27.61 | 11.77x (+1077.3%) |
| CPU Hash map lookup (minimal) | minimal | 343.76 | 27.61 | 12.45x (+1145.1%) |
| CPU Hash map delete (minimal) | minimal | 318.81 | 27.61 | 11.55x (+1054.7%) |
| Empty probe (tiny) | tiny | 35.26 | 27.61 | 1.28x (+27.7%) |
| GPU Array map update (small) | small | 36.27 | 27.40 | 1.32x (+32.4%) |
| CUDA Counter (minimal) | minimal | 3453.82 | 38.31 | 90.15x (+8915.5%) |
| Kernel Retsnoop (minimal) | minimal | 46.51 | 38.31 | 1.21x (+21.4%) |


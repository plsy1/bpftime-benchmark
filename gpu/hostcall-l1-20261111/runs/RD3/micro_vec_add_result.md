
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:32:53.104759  

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
| Baseline (tiny) | tiny | 27.50 | - | - |
| Baseline (small) | small | 28.70 | - | - |
| Baseline (minimal) | minimal | 38.45 | - | - |
| CPU Array map update (minimal) | minimal | 321.78 | 27.50 | 11.70x (+1070.1%) |
| CPU Array map lookup (minimal) | minimal | 375.12 | 27.50 | 13.64x (+1264.1%) |
| CPU Hash map update (minimal) | minimal | 335.66 | 27.50 | 12.21x (+1120.6%) |
| CPU Hash map lookup (minimal) | minimal | 341.31 | 27.50 | 12.41x (+1141.1%) |
| CPU Hash map delete (minimal) | minimal | 309.70 | 27.50 | 11.26x (+1026.2%) |
| Empty probe (tiny) | tiny | 35.24 | 27.50 | 1.28x (+28.1%) |
| GPU Array map update (small) | small | 37.04 | 28.70 | 1.29x (+29.1%) |
| CUDA Counter (minimal) | minimal | 3442.95 | 38.45 | 89.54x (+8854.4%) |
| Kernel Retsnoop (minimal) | minimal | 46.60 | 38.45 | 1.21x (+21.2%) |


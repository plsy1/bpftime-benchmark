
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:42:46.842728  

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
| Baseline (tiny) | tiny | 27.13 | - | - |
| Baseline (small) | small | 27.61 | - | - |
| Baseline (minimal) | minimal | 37.60 | - | - |
| CPU Array map update (minimal) | minimal | 48.13 | 27.13 | 1.77x (+77.4%) |
| CPU Array map lookup (minimal) | minimal | 44.05 | 27.13 | 1.62x (+62.4%) |
| CPU Hash map update (minimal) | minimal | 329.94 | 27.13 | 12.16x (+1116.1%) |
| CPU Hash map lookup (minimal) | minimal | 343.64 | 27.13 | 12.67x (+1166.6%) |
| CPU Hash map delete (minimal) | minimal | 318.42 | 27.13 | 11.74x (+1073.7%) |
| Empty probe (tiny) | tiny | 31.70 | 27.13 | 1.17x (+16.8%) |
| GPU Array map update (small) | small | 35.38 | 27.61 | 1.28x (+28.1%) |
| CUDA Counter (minimal) | minimal | 2877.20 | 37.60 | 76.52x (+7552.1%) |
| Kernel Retsnoop (minimal) | minimal | 52.02 | 37.60 | 1.38x (+38.4%) |


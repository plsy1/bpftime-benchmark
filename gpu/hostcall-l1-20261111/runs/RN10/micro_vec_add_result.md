
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:56:00.307975  

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
| Baseline (tiny) | tiny | 26.24 | - | - |
| Baseline (small) | small | 27.55 | - | - |
| Baseline (minimal) | minimal | 37.34 | - | - |
| CPU Array map update (minimal) | minimal | 47.36 | 26.24 | 1.80x (+80.5%) |
| CPU Array map lookup (minimal) | minimal | 43.21 | 26.24 | 1.65x (+64.7%) |
| CPU Hash map update (minimal) | minimal | 326.30 | 26.24 | 12.44x (+1143.5%) |
| CPU Hash map lookup (minimal) | minimal | 334.22 | 26.24 | 12.74x (+1173.7%) |
| CPU Hash map delete (minimal) | minimal | 325.48 | 26.24 | 12.40x (+1140.4%) |
| Empty probe (tiny) | tiny | 34.45 | 26.24 | 1.31x (+31.3%) |
| GPU Array map update (small) | small | 35.30 | 27.55 | 1.28x (+28.1%) |
| CUDA Counter (minimal) | minimal | 2877.71 | 37.34 | 77.07x (+7606.8%) |
| Kernel Retsnoop (minimal) | minimal | 47.96 | 37.34 | 1.28x (+28.4%) |


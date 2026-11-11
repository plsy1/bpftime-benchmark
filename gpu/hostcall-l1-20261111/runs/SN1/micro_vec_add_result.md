
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:24:12.442323  

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
| Baseline (tiny) | tiny | 27.15 | - | - |
| Baseline (small) | small | 25.90 | - | - |
| Baseline (minimal) | minimal | 44.82 | - | - |
| CPU Array map update (minimal) | minimal | 56.61 | 27.15 | 2.09x (+108.5%) |
| CPU Array map lookup (minimal) | minimal | 56.76 | 27.15 | 2.09x (+109.1%) |
| CPU Hash map update (minimal) | minimal | 447.78 | 27.15 | 16.49x (+1549.3%) |
| CPU Hash map lookup (minimal) | minimal | 474.66 | 27.15 | 17.48x (+1648.3%) |
| CPU Hash map delete (minimal) | minimal | 449.96 | 27.15 | 16.57x (+1557.3%) |
| Empty probe (tiny) | tiny | 42.29 | 27.15 | 1.56x (+55.8%) |
| GPU Array map update (small) | small | 45.19 | 25.90 | 1.74x (+74.5%) |
| CUDA Counter (minimal) | minimal | 3838.12 | 44.82 | 85.63x (+8463.4%) |
| Kernel Retsnoop (minimal) | minimal | 58.74 | 44.82 | 1.31x (+31.1%) |


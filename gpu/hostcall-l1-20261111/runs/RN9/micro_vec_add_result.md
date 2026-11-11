
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:54:21.089916  

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
| Baseline (tiny) | tiny | 25.94 | - | - |
| Baseline (small) | small | 26.12 | - | - |
| Baseline (minimal) | minimal | 40.34 | - | - |
| CPU Array map update (minimal) | minimal | 47.34 | 25.94 | 1.82x (+82.5%) |
| CPU Array map lookup (minimal) | minimal | 49.46 | 25.94 | 1.91x (+90.7%) |
| CPU Hash map update (minimal) | minimal | 336.81 | 25.94 | 12.98x (+1198.4%) |
| CPU Hash map lookup (minimal) | minimal | 340.30 | 25.94 | 13.12x (+1211.9%) |
| CPU Hash map delete (minimal) | minimal | 317.46 | 25.94 | 12.24x (+1123.8%) |
| Empty probe (tiny) | tiny | 31.65 | 25.94 | 1.22x (+22.0%) |
| GPU Array map update (small) | small | 35.72 | 26.12 | 1.37x (+36.8%) |
| CUDA Counter (minimal) | minimal | 2958.56 | 40.34 | 73.34x (+7234.1%) |
| Kernel Retsnoop (minimal) | minimal | 47.55 | 40.34 | 1.18x (+17.9%) |


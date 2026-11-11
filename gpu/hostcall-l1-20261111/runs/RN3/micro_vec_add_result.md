
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:34:31.808327  

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
| Baseline (tiny) | tiny | 26.90 | - | - |
| Baseline (small) | small | 27.43 | - | - |
| Baseline (minimal) | minimal | 39.30 | - | - |
| CPU Array map update (minimal) | minimal | 43.76 | 26.90 | 1.63x (+62.7%) |
| CPU Array map lookup (minimal) | minimal | 43.71 | 26.90 | 1.62x (+62.5%) |
| CPU Hash map update (minimal) | minimal | 318.20 | 26.90 | 11.83x (+1082.9%) |
| CPU Hash map lookup (minimal) | minimal | 341.20 | 26.90 | 12.68x (+1168.4%) |
| CPU Hash map delete (minimal) | minimal | 317.96 | 26.90 | 11.82x (+1082.0%) |
| Empty probe (tiny) | tiny | 32.85 | 26.90 | 1.22x (+22.1%) |
| GPU Array map update (small) | small | 38.65 | 27.43 | 1.41x (+40.9%) |
| CUDA Counter (minimal) | minimal | 2869.21 | 39.30 | 73.01x (+7200.8%) |
| Kernel Retsnoop (minimal) | minimal | 47.34 | 39.30 | 1.20x (+20.5%) |


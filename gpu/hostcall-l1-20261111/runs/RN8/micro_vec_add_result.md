
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:49:23.437297  

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
| Baseline (tiny) | tiny | 28.01 | - | - |
| Baseline (small) | small | 27.40 | - | - |
| Baseline (minimal) | minimal | 41.15 | - | - |
| CPU Array map update (minimal) | minimal | 47.12 | 28.01 | 1.68x (+68.2%) |
| CPU Array map lookup (minimal) | minimal | 51.19 | 28.01 | 1.83x (+82.8%) |
| CPU Hash map update (minimal) | minimal | 326.94 | 28.01 | 11.67x (+1067.2%) |
| CPU Hash map lookup (minimal) | minimal | 337.64 | 28.01 | 12.05x (+1105.4%) |
| CPU Hash map delete (minimal) | minimal | 313.81 | 28.01 | 11.20x (+1020.3%) |
| Empty probe (tiny) | tiny | 36.03 | 28.01 | 1.29x (+28.6%) |
| GPU Array map update (small) | small | 39.07 | 27.40 | 1.43x (+42.6%) |
| CUDA Counter (minimal) | minimal | 2852.48 | 41.15 | 69.32x (+6831.9%) |
| Kernel Retsnoop (minimal) | minimal | 48.98 | 41.15 | 1.19x (+19.0%) |


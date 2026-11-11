
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:57:39.550955  

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
| Baseline (tiny) | tiny | 28.04 | - | - |
| Baseline (small) | small | 27.93 | - | - |
| Baseline (minimal) | minimal | 38.19 | - | - |
| CPU Array map update (minimal) | minimal | 317.82 | 28.04 | 11.33x (+1033.5%) |
| CPU Array map lookup (minimal) | minimal | 375.54 | 28.04 | 13.39x (+1239.3%) |
| CPU Hash map update (minimal) | minimal | 322.44 | 28.04 | 11.50x (+1049.9%) |
| CPU Hash map lookup (minimal) | minimal | 337.21 | 28.04 | 12.03x (+1102.6%) |
| CPU Hash map delete (minimal) | minimal | 317.29 | 28.04 | 11.32x (+1031.6%) |
| Empty probe (tiny) | tiny | 32.79 | 28.04 | 1.17x (+16.9%) |
| GPU Array map update (small) | small | 37.70 | 27.93 | 1.35x (+35.0%) |
| CUDA Counter (minimal) | minimal | 3502.88 | 38.19 | 91.72x (+9072.2%) |
| Kernel Retsnoop (minimal) | minimal | 48.63 | 38.19 | 1.27x (+27.3%) |


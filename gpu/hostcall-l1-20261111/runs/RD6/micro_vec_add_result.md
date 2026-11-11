
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:44:26.064380  

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
| Baseline (tiny) | tiny | 27.28 | - | - |
| Baseline (small) | small | 27.94 | - | - |
| Baseline (minimal) | minimal | 28.17 | - | - |
| CPU Array map update (minimal) | minimal | 323.32 | 27.28 | 11.85x (+1085.2%) |
| CPU Array map lookup (minimal) | minimal | 367.22 | 27.28 | 13.46x (+1246.1%) |
| CPU Hash map update (minimal) | minimal | 331.56 | 27.28 | 12.15x (+1115.4%) |
| CPU Hash map lookup (minimal) | minimal | 336.08 | 27.28 | 12.32x (+1132.0%) |
| CPU Hash map delete (minimal) | minimal | 326.01 | 27.28 | 11.95x (+1095.1%) |
| Empty probe (tiny) | tiny | 30.64 | 27.28 | 1.12x (+12.3%) |
| GPU Array map update (small) | small | 35.06 | 27.94 | 1.25x (+25.5%) |
| CUDA Counter (minimal) | minimal | 3462.85 | 28.17 | 122.93x (+12192.7%) |
| Kernel Retsnoop (minimal) | minimal | 51.93 | 28.17 | 1.84x (+84.3%) |


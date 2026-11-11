
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:22:32.708940  

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
| Baseline (tiny) | tiny | 26.60 | - | - |
| Baseline (small) | small | 27.14 | - | - |
| Baseline (minimal) | minimal | 39.48 | - | - |
| CPU Array map update (minimal) | minimal | 445.29 | 26.60 | 16.74x (+1574.0%) |
| CPU Array map lookup (minimal) | minimal | 505.16 | 26.60 | 18.99x (+1799.1%) |
| CPU Hash map update (minimal) | minimal | 445.21 | 26.60 | 16.74x (+1573.7%) |
| CPU Hash map lookup (minimal) | minimal | 466.02 | 26.60 | 17.52x (+1652.0%) |
| CPU Hash map delete (minimal) | minimal | 451.50 | 26.60 | 16.97x (+1597.4%) |
| Empty probe (tiny) | tiny | 37.33 | 26.60 | 1.40x (+40.3%) |
| GPU Array map update (small) | small | 47.62 | 27.14 | 1.75x (+75.5%) |
| CUDA Counter (minimal) | minimal | 4720.74 | 39.48 | 119.57x (+11857.3%) |
| Kernel Retsnoop (minimal) | minimal | 56.92 | 39.48 | 1.44x (+44.2%) |



# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:36:10.529688  

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
| Baseline (tiny) | tiny | 26.93 | - | - |
| Baseline (small) | small | 26.00 | - | - |
| Baseline (minimal) | minimal | 40.93 | - | - |
| CPU Array map update (minimal) | minimal | 44.96 | 26.93 | 1.67x (+67.0%) |
| CPU Array map lookup (minimal) | minimal | 46.95 | 26.93 | 1.74x (+74.3%) |
| CPU Hash map update (minimal) | minimal | 328.01 | 26.93 | 12.18x (+1118.0%) |
| CPU Hash map lookup (minimal) | minimal | 340.93 | 26.93 | 12.66x (+1166.0%) |
| CPU Hash map delete (minimal) | minimal | 312.22 | 26.93 | 11.59x (+1059.4%) |
| Empty probe (tiny) | tiny | 35.20 | 26.93 | 1.31x (+30.7%) |
| GPU Array map update (small) | small | 38.99 | 26.00 | 1.50x (+50.0%) |
| CUDA Counter (minimal) | minimal | 2866.17 | 40.93 | 70.03x (+6902.6%) |
| Kernel Retsnoop (minimal) | minimal | 46.43 | 40.93 | 1.13x (+13.4%) |


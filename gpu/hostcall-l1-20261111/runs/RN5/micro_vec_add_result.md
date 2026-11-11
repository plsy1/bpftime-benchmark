
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:41:07.643320  

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
| Baseline (tiny) | tiny | 27.39 | - | - |
| Baseline (small) | small | 27.19 | - | - |
| Baseline (minimal) | minimal | 39.58 | - | - |
| CPU Array map update (minimal) | minimal | 45.26 | 27.39 | 1.65x (+65.2%) |
| CPU Array map lookup (minimal) | minimal | 54.24 | 27.39 | 1.98x (+98.0%) |
| CPU Hash map update (minimal) | minimal | 318.19 | 27.39 | 11.62x (+1061.7%) |
| CPU Hash map lookup (minimal) | minimal | 338.81 | 27.39 | 12.37x (+1137.0%) |
| CPU Hash map delete (minimal) | minimal | 313.34 | 27.39 | 11.44x (+1044.0%) |
| Empty probe (tiny) | tiny | 34.30 | 27.39 | 1.25x (+25.2%) |
| GPU Array map update (small) | small | 35.75 | 27.19 | 1.31x (+31.5%) |
| CUDA Counter (minimal) | minimal | 2861.98 | 39.58 | 72.31x (+7130.9%) |
| Kernel Retsnoop (minimal) | minimal | 47.84 | 39.58 | 1.21x (+20.9%) |



# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:26:17.568006  

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
| Baseline (tiny) | tiny | 27.18 | - | - |
| Baseline (small) | small | 27.31 | - | - |
| Baseline (minimal) | minimal | 40.64 | - | - |
| CPU Array map update (minimal) | minimal | 316.82 | 27.18 | 11.66x (+1065.6%) |
| CPU Array map lookup (minimal) | minimal | 388.66 | 27.18 | 14.30x (+1329.9%) |
| CPU Hash map update (minimal) | minimal | 324.55 | 27.18 | 11.94x (+1094.1%) |
| CPU Hash map lookup (minimal) | minimal | 333.38 | 27.18 | 12.27x (+1126.6%) |
| CPU Hash map delete (minimal) | minimal | 314.32 | 27.18 | 11.56x (+1056.4%) |
| Empty probe (tiny) | tiny | 34.19 | 27.18 | 1.26x (+25.8%) |
| GPU Array map update (small) | small | 38.07 | 27.31 | 1.39x (+39.4%) |
| CUDA Counter (minimal) | minimal | 3535.33 | 40.64 | 86.99x (+8599.1%) |
| Kernel Retsnoop (minimal) | minimal | 46.84 | 40.64 | 1.15x (+15.3%) |


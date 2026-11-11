
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:29:35.422403  

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
| Baseline (tiny) | tiny | 27.12 | - | - |
| Baseline (small) | small | 26.17 | - | - |
| Baseline (minimal) | minimal | 40.20 | - | - |
| CPU Array map update (minimal) | minimal | 47.12 | 27.12 | 1.74x (+73.7%) |
| CPU Array map lookup (minimal) | minimal | 45.59 | 27.12 | 1.68x (+68.1%) |
| CPU Hash map update (minimal) | minimal | 322.45 | 27.12 | 11.89x (+1089.0%) |
| CPU Hash map lookup (minimal) | minimal | 346.05 | 27.12 | 12.76x (+1176.0%) |
| CPU Hash map delete (minimal) | minimal | 323.57 | 27.12 | 11.93x (+1093.1%) |
| Empty probe (tiny) | tiny | 34.49 | 27.12 | 1.27x (+27.2%) |
| GPU Array map update (small) | small | 40.05 | 26.17 | 1.53x (+53.0%) |
| CUDA Counter (minimal) | minimal | 2989.12 | 40.20 | 74.36x (+7335.6%) |
| Kernel Retsnoop (minimal) | minimal | 48.86 | 40.20 | 1.22x (+21.5%) |


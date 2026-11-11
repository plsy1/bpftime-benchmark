
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:46:05.089071  

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
| Baseline (tiny) | tiny | 27.53 | - | - |
| Baseline (small) | small | 27.67 | - | - |
| Baseline (minimal) | minimal | 39.63 | - | - |
| CPU Array map update (minimal) | minimal | 394.55 | 27.53 | 14.33x (+1333.2%) |
| CPU Array map lookup (minimal) | minimal | 374.60 | 27.53 | 13.61x (+1260.7%) |
| CPU Hash map update (minimal) | minimal | 316.83 | 27.53 | 11.51x (+1050.9%) |
| CPU Hash map lookup (minimal) | minimal | 341.81 | 27.53 | 12.42x (+1141.6%) |
| CPU Hash map delete (minimal) | minimal | 322.52 | 27.53 | 11.72x (+1071.5%) |
| Empty probe (tiny) | tiny | 33.55 | 27.53 | 1.22x (+21.9%) |
| GPU Array map update (small) | small | 38.07 | 27.67 | 1.38x (+37.6%) |
| CUDA Counter (minimal) | minimal | 3554.73 | 39.63 | 89.70x (+8869.8%) |
| Kernel Retsnoop (minimal) | minimal | 48.51 | 39.63 | 1.22x (+22.4%) |


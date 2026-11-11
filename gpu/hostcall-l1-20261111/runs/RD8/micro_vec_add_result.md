
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:51:02.713736  

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
| Baseline (tiny) | tiny | 27.41 | - | - |
| Baseline (small) | small | 27.28 | - | - |
| Baseline (minimal) | minimal | 39.67 | - | - |
| CPU Array map update (minimal) | minimal | 315.52 | 27.41 | 11.51x (+1051.1%) |
| CPU Array map lookup (minimal) | minimal | 370.57 | 27.41 | 13.52x (+1252.0%) |
| CPU Hash map update (minimal) | minimal | 320.28 | 27.41 | 11.68x (+1068.5%) |
| CPU Hash map lookup (minimal) | minimal | 340.59 | 27.41 | 12.43x (+1142.6%) |
| CPU Hash map delete (minimal) | minimal | 315.63 | 27.41 | 11.52x (+1051.5%) |
| Empty probe (tiny) | tiny | 32.70 | 27.41 | 1.19x (+19.3%) |
| GPU Array map update (small) | small | 33.09 | 27.28 | 1.21x (+21.3%) |
| CUDA Counter (minimal) | minimal | 3460.35 | 39.67 | 87.23x (+8622.8%) |
| Kernel Retsnoop (minimal) | minimal | 48.25 | 39.67 | 1.22x (+21.6%) |


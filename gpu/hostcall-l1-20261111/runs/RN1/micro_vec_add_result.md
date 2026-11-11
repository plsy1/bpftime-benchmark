
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:27:56.388629  

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
| Baseline (tiny) | tiny | 27.37 | - | - |
| Baseline (small) | small | 27.90 | - | - |
| Baseline (minimal) | minimal | 43.82 | - | - |
| CPU Array map update (minimal) | minimal | 47.60 | 27.37 | 1.74x (+73.9%) |
| CPU Array map lookup (minimal) | minimal | 51.88 | 27.37 | 1.90x (+89.6%) |
| CPU Hash map update (minimal) | minimal | 344.39 | 27.37 | 12.58x (+1158.3%) |
| CPU Hash map lookup (minimal) | minimal | 346.19 | 27.37 | 12.65x (+1164.9%) |
| CPU Hash map delete (minimal) | minimal | 318.34 | 27.37 | 11.63x (+1063.1%) |
| Empty probe (tiny) | tiny | 32.99 | 27.37 | 1.21x (+20.5%) |
| GPU Array map update (small) | small | 35.51 | 27.90 | 1.27x (+27.3%) |
| CUDA Counter (minimal) | minimal | 2885.76 | 43.82 | 65.85x (+6485.5%) |
| Kernel Retsnoop (minimal) | minimal | 46.63 | 43.82 | 1.06x (+6.4%) |


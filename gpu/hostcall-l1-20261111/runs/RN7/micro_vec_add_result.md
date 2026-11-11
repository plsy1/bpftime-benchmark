
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:47:44.280149  

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
| Baseline (tiny) | tiny | 27.15 | - | - |
| Baseline (small) | small | 27.20 | - | - |
| Baseline (minimal) | minimal | 35.36 | - | - |
| CPU Array map update (minimal) | minimal | 49.43 | 27.15 | 1.82x (+82.1%) |
| CPU Array map lookup (minimal) | minimal | 52.58 | 27.15 | 1.94x (+93.7%) |
| CPU Hash map update (minimal) | minimal | 356.93 | 27.15 | 13.15x (+1214.7%) |
| CPU Hash map lookup (minimal) | minimal | 341.42 | 27.15 | 12.58x (+1157.5%) |
| CPU Hash map delete (minimal) | minimal | 311.80 | 27.15 | 11.48x (+1048.4%) |
| Empty probe (tiny) | tiny | 29.40 | 27.15 | 1.08x (+8.3%) |
| GPU Array map update (small) | small | 33.24 | 27.20 | 1.22x (+22.2%) |
| CUDA Counter (minimal) | minimal | 2949.37 | 35.36 | 83.41x (+8241.0%) |
| Kernel Retsnoop (minimal) | minimal | 51.91 | 35.36 | 1.47x (+46.8%) |


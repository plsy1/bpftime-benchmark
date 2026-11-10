
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T23:59:58.429118  

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
| xlarge_n32 | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add_legacy | 32 | 1000 | 512 | 1954 |

## Benchmark Results

| Test Name | Workload | Avg Time (μs) | vs Baseline | Overhead |
|-----------|----------|---------------|-------------|----------|
| Baseline (large) | large | 30.53 | - | - |
| Baseline (xlarge) | xlarge | 158.44 | - | - |
| Baseline (xlarge_n32) | xlarge_n32 | 48.86 | - | - |
| GPU Array map update (large) | large | 244.38 | 30.53 | 8.00x (+700.5%) |
| GPU Array map update (xlarge) | xlarge | 1506.02 | 158.44 | 9.51x (+850.5%) |
| GPU Array map update (xlarge_n32) | xlarge_n32 | 2176.47 | 48.86 | 44.55x (+4354.5%) |


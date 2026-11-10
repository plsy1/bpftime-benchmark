
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T00:02:01.440551  

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
| Baseline (large) | large | 31.54 | - | - |
| Baseline (xlarge) | xlarge | 160.96 | - | - |
| Baseline (xlarge_n32) | xlarge_n32 | 49.29 | - | - |
| GPU Array map update (large) | large | 249.28 | 31.54 | 7.90x (+690.4%) |
| GPU Array map update (xlarge) | xlarge | 1476.01 | 160.96 | 9.17x (+817.0%) |
| GPU Array map update (xlarge_n32) | xlarge_n32 | 2136.35 | 49.29 | 43.34x (+4234.2%) |



# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T23:49:21.104533  

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
| Baseline (large) | large | 31.03 | - | - |
| Baseline (xlarge) | xlarge | 159.77 | - | - |
| Baseline (xlarge_n32) | xlarge_n32 | 49.42 | - | - |
| GPU Array map update (large) | large | 234.51 | 31.03 | 7.56x (+655.8%) |
| GPU Array map update (xlarge) | xlarge | 1574.74 | 159.77 | 9.86x (+885.6%) |
| GPU Array map update (xlarge_n32) | xlarge_n32 | 1911.43 | 49.42 | 38.68x (+3767.7%) |


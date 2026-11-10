
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T23:45:46.469032  

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
| Baseline (large) | large | 30.63 | - | - |
| Baseline (xlarge) | xlarge | 158.89 | - | - |
| Baseline (xlarge_n32) | xlarge_n32 | 50.46 | - | - |
| GPU Array map update (large) | large | 223.69 | 30.63 | 7.30x (+630.3%) |
| GPU Array map update (xlarge) | xlarge | 1750.77 | 158.89 | 11.02x (+1001.9%) |
| GPU Array map update (xlarge_n32) | xlarge_n32 | 1841.72 | 50.46 | 36.50x (+3549.9%) |


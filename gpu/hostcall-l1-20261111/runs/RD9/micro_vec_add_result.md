
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T12:52:41.917589  

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
| Baseline (tiny) | tiny | 25.96 | - | - |
| Baseline (small) | small | 26.25 | - | - |
| Baseline (minimal) | minimal | 37.54 | - | - |
| CPU Array map update (minimal) | minimal | 314.92 | 25.96 | 12.13x (+1113.1%) |
| CPU Array map lookup (minimal) | minimal | 377.88 | 25.96 | 14.56x (+1355.6%) |
| CPU Hash map update (minimal) | minimal | 324.03 | 25.96 | 12.48x (+1148.2%) |
| CPU Hash map lookup (minimal) | minimal | 339.57 | 25.96 | 13.08x (+1208.1%) |
| CPU Hash map delete (minimal) | minimal | 319.09 | 25.96 | 12.29x (+1129.2%) |
| Empty probe (tiny) | tiny | 34.44 | 25.96 | 1.33x (+32.7%) |
| GPU Array map update (small) | small | 32.52 | 26.25 | 1.24x (+23.9%) |
| CUDA Counter (minimal) | minimal | 3462.60 | 37.54 | 92.24x (+9123.8%) |
| Kernel Retsnoop (minimal) | minimal | 53.69 | 37.54 | 1.43x (+43.0%) |


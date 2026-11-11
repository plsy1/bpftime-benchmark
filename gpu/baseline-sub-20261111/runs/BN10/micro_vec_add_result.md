
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:29:21.276125  

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
| Baseline (small) | small | 27.81 | - | - |
| Baseline (large) | large | 31.23 | - | - |
| Baseline (xlarge) | xlarge | 158.22 | - | - |
| Baseline (minimal) | minimal | 45.92 | - | - |
| Empty probe (tiny) | tiny | 31.85 | 27.53 | 1.16x (+15.7%) |
| Empty probe (xlarge) | xlarge | 171.68 | 158.22 | 1.09x (+8.5%) |
| Global timer (xlarge) | xlarge | 173.82 | 158.22 | 1.10x (+9.9%) |
| GPU Ringbuf (small) | small | 44.64 | 27.81 | 1.61x (+60.5%) |
| GPU Ringbuf (xlarge) | xlarge | 211.17 | 158.22 | 1.33x (+33.5%) |
| Per-GPU-thread array (small) | small | 33.56 | 27.81 | 1.21x (+20.7%) |
| Memtrace (small) | small | 31.62 | 27.81 | 1.14x (+13.7%) |
| Memtrace (large) | large | 175.93 | 31.23 | 5.63x (+463.3%) |
| GPU Array map update (small) | small | 40.35 | 27.81 | 1.45x (+45.1%) |
| GPU Array map update (large) | large | 243.18 | 31.23 | 7.79x (+678.7%) |
| GPU Array map lookup (small) | small | 32.82 | 27.81 | 1.18x (+18.0%) |
| GPU Array map lookup (large) | large | 65.42 | 31.23 | 2.09x (+109.5%) |
| CPU Array map update (minimal) | minimal | 50.64 | 27.53 | 1.84x (+83.9%) |
| CPU Array map lookup (minimal) | minimal | 46.09 | 27.53 | 1.67x (+67.4%) |
| CUDA Counter (minimal) | minimal | 2842.13 | 45.92 | 61.89x (+6089.3%) |
| Kernel Retsnoop (minimal) | minimal | 54.89 | 45.92 | 1.20x (+19.5%) |
| Thread Histogram (minimal) | minimal | 49.19 | 45.92 | 1.07x (+7.1%) |
| Memory Trace (minimal) | minimal | 58.89 | 45.92 | 1.28x (+28.2%) |


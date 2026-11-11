
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:46:53.187924  

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
| Baseline (tiny) | tiny | 27.24 | - | - |
| Baseline (small) | small | 28.02 | - | - |
| Baseline (large) | large | 32.43 | - | - |
| Baseline (xlarge) | xlarge | 162.34 | - | - |
| Baseline (minimal) | minimal | 39.34 | - | - |
| Empty probe (tiny) | tiny | 35.18 | 27.24 | 1.29x (+29.1%) |
| Empty probe (xlarge) | xlarge | 175.31 | 162.34 | 1.08x (+8.0%) |
| Global timer (xlarge) | xlarge | 179.28 | 162.34 | 1.10x (+10.4%) |
| GPU Ringbuf (small) | small | 45.49 | 28.02 | 1.62x (+62.3%) |
| GPU Ringbuf (xlarge) | xlarge | 215.13 | 162.34 | 1.33x (+32.5%) |
| Per-GPU-thread array (small) | small | 33.29 | 28.02 | 1.19x (+18.8%) |
| Memtrace (small) | small | 32.89 | 28.02 | 1.17x (+17.4%) |
| Memtrace (large) | large | 173.46 | 32.43 | 5.35x (+434.9%) |
| GPU Array map update (small) | small | 38.56 | 28.02 | 1.38x (+37.6%) |
| GPU Array map update (large) | large | 247.23 | 32.43 | 7.62x (+662.3%) |
| GPU Array map lookup (small) | small | 38.17 | 28.02 | 1.36x (+36.2%) |
| GPU Array map lookup (large) | large | 66.35 | 32.43 | 2.05x (+104.6%) |
| CPU Array map update (minimal) | minimal | 46.76 | 27.24 | 1.72x (+71.7%) |
| CPU Array map lookup (minimal) | minimal | 45.50 | 27.24 | 1.67x (+67.0%) |
| CUDA Counter (minimal) | minimal | 2861.21 | 39.34 | 72.73x (+7173.0%) |
| Kernel Retsnoop (minimal) | minimal | 51.08 | 39.34 | 1.30x (+29.8%) |
| Thread Histogram (minimal) | minimal | 50.23 | 39.34 | 1.28x (+27.7%) |
| Memory Trace (minimal) | minimal | 65.67 | 39.34 | 1.67x (+66.9%) |


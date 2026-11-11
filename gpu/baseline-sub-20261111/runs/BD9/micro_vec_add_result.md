
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:27:24.676521  

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
| Baseline (tiny) | tiny | 27.01 | - | - |
| Baseline (small) | small | 25.60 | - | - |
| Baseline (large) | large | 30.45 | - | - |
| Baseline (xlarge) | xlarge | 159.28 | - | - |
| Baseline (minimal) | minimal | 39.30 | - | - |
| Empty probe (tiny) | tiny | 34.86 | 27.01 | 1.29x (+29.1%) |
| Empty probe (xlarge) | xlarge | 173.74 | 159.28 | 1.09x (+9.1%) |
| Global timer (xlarge) | xlarge | 180.34 | 159.28 | 1.13x (+13.2%) |
| GPU Ringbuf (small) | small | 42.89 | 25.60 | 1.68x (+67.5%) |
| GPU Ringbuf (xlarge) | xlarge | 216.43 | 159.28 | 1.36x (+35.9%) |
| Per-GPU-thread array (small) | small | 38.70 | 25.60 | 1.51x (+51.2%) |
| Memtrace (small) | small | 34.28 | 25.60 | 1.34x (+33.9%) |
| Memtrace (large) | large | 170.26 | 30.45 | 5.59x (+459.1%) |
| GPU Array map update (small) | small | 37.92 | 25.60 | 1.48x (+48.1%) |
| GPU Array map update (large) | large | 234.07 | 30.45 | 7.69x (+668.7%) |
| GPU Array map lookup (small) | small | 33.07 | 25.60 | 1.29x (+29.2%) |
| GPU Array map lookup (large) | large | 65.47 | 30.45 | 2.15x (+115.0%) |
| CPU Array map update (minimal) | minimal | 318.98 | 27.01 | 11.81x (+1081.0%) |
| CPU Array map lookup (minimal) | minimal | 369.53 | 27.01 | 13.68x (+1268.1%) |
| CUDA Counter (minimal) | minimal | 3454.36 | 39.30 | 87.90x (+8689.7%) |
| Kernel Retsnoop (minimal) | minimal | 53.04 | 39.30 | 1.35x (+35.0%) |
| Thread Histogram (minimal) | minimal | 47.21 | 39.30 | 1.20x (+20.1%) |
| Memory Trace (minimal) | minimal | 53.69 | 39.30 | 1.37x (+36.6%) |


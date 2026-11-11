
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:48:04.193572  

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
| Baseline (tiny) | tiny | 26.89 | - | - |
| Baseline (small) | small | 27.76 | - | - |
| Baseline (large) | large | 30.53 | - | - |
| Baseline (xlarge) | xlarge | 161.40 | - | - |
| Baseline (minimal) | minimal | 39.99 | - | - |
| Empty probe (tiny) | tiny | 29.49 | 26.89 | 1.10x (+9.7%) |
| Empty probe (xlarge) | xlarge | 174.03 | 161.40 | 1.08x (+7.8%) |
| Global timer (xlarge) | xlarge | 178.99 | 161.40 | 1.11x (+10.9%) |
| GPU Ringbuf (small) | small | 40.62 | 27.76 | 1.46x (+46.3%) |
| GPU Ringbuf (xlarge) | xlarge | 214.34 | 161.40 | 1.33x (+32.8%) |
| Per-GPU-thread array (small) | small | 36.22 | 27.76 | 1.30x (+30.5%) |
| Memtrace (small) | small | 35.57 | 27.76 | 1.28x (+28.1%) |
| Memtrace (large) | large | 170.73 | 30.53 | 5.59x (+459.2%) |
| GPU Array map update (small) | small | 36.22 | 27.76 | 1.30x (+30.5%) |
| GPU Array map update (large) | large | 235.40 | 30.53 | 7.71x (+671.0%) |
| GPU Array map lookup (small) | small | 37.76 | 27.76 | 1.36x (+36.0%) |
| GPU Array map lookup (large) | large | 67.74 | 30.53 | 2.22x (+121.9%) |
| CPU Array map update (minimal) | minimal | 48.34 | 26.89 | 1.80x (+79.8%) |
| CPU Array map lookup (minimal) | minimal | 47.22 | 26.89 | 1.76x (+75.6%) |
| CUDA Counter (minimal) | minimal | 3045.76 | 39.99 | 76.16x (+7516.3%) |
| Kernel Retsnoop (minimal) | minimal | 53.27 | 39.99 | 1.33x (+33.2%) |
| Thread Histogram (minimal) | minimal | 43.95 | 39.99 | 1.10x (+9.9%) |
| Memory Trace (minimal) | minimal | 53.11 | 39.99 | 1.33x (+32.8%) |


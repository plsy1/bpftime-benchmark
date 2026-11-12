
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:28:52.623643  

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
| Baseline (tiny) | tiny | 27.30 | - | - |
| Baseline (small) | small | 27.52 | - | - |
| Baseline (large) | large | 31.24 | - | - |
| Baseline (xlarge) | xlarge | 159.03 | - | - |
| Baseline (minimal) | minimal | 37.41 | - | - |
| Empty probe (tiny) | tiny | 31.53 | 27.30 | 1.15x (+15.5%) |
| Empty probe (xlarge) | xlarge | 172.06 | 159.03 | 1.08x (+8.2%) |
| Global timer (xlarge) | xlarge | 181.88 | 159.03 | 1.14x (+14.4%) |
| GPU Ringbuf (small) | small | 46.34 | 27.52 | 1.68x (+68.4%) |
| GPU Ringbuf (xlarge) | xlarge | 216.95 | 159.03 | 1.36x (+36.4%) |
| Per-GPU-thread array (small) | small | 35.60 | 27.52 | 1.29x (+29.4%) |
| Memtrace (small) | small | 35.75 | 27.52 | 1.30x (+29.9%) |
| Memtrace (large) | large | 177.13 | 31.24 | 5.67x (+467.0%) |
| GPU Array map update (small) | small | 36.09 | 27.52 | 1.31x (+31.1%) |
| GPU Array map update (large) | large | 234.06 | 31.24 | 7.49x (+649.2%) |
| GPU Array map lookup (small) | small | 30.95 | 27.52 | 1.12x (+12.5%) |
| GPU Array map lookup (large) | large | 68.97 | 31.24 | 2.21x (+120.8%) |
| CPU Array map update (minimal) | minimal | 36.08 | 27.30 | 1.32x (+32.2%) |
| CPU Array map lookup (minimal) | minimal | 45.90 | 27.30 | 1.68x (+68.1%) |
| CUDA Counter (minimal) | minimal | 2858.94 | 37.41 | 76.42x (+7542.2%) |
| Kernel Retsnoop (minimal) | minimal | 50.75 | 37.41 | 1.36x (+35.7%) |
| Thread Histogram (minimal) | minimal | 47.06 | 37.41 | 1.26x (+25.8%) |
| Memory Trace (minimal) | minimal | 63.96 | 37.41 | 1.71x (+71.0%) |


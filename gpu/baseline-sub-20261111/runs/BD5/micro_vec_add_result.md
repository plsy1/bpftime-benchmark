
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:18:06.437138  

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
| Baseline (tiny) | tiny | 27.51 | - | - |
| Baseline (small) | small | 27.73 | - | - |
| Baseline (large) | large | 31.24 | - | - |
| Baseline (xlarge) | xlarge | 160.42 | - | - |
| Baseline (minimal) | minimal | 35.40 | - | - |
| Empty probe (tiny) | tiny | 32.63 | 27.51 | 1.19x (+18.6%) |
| Empty probe (xlarge) | xlarge | 168.29 | 160.42 | 1.05x (+4.9%) |
| Global timer (xlarge) | xlarge | 175.84 | 160.42 | 1.10x (+9.6%) |
| GPU Ringbuf (small) | small | 44.58 | 27.73 | 1.61x (+60.8%) |
| GPU Ringbuf (xlarge) | xlarge | 212.88 | 160.42 | 1.33x (+32.7%) |
| Per-GPU-thread array (small) | small | 31.70 | 27.73 | 1.14x (+14.3%) |
| Memtrace (small) | small | 33.50 | 27.73 | 1.21x (+20.8%) |
| Memtrace (large) | large | 171.33 | 31.24 | 5.48x (+448.4%) |
| GPU Array map update (small) | small | 36.36 | 27.73 | 1.31x (+31.1%) |
| GPU Array map update (large) | large | 249.56 | 31.24 | 7.99x (+698.8%) |
| GPU Array map lookup (small) | small | 34.73 | 27.73 | 1.25x (+25.2%) |
| GPU Array map lookup (large) | large | 69.61 | 31.24 | 2.23x (+122.8%) |
| CPU Array map update (minimal) | minimal | 315.64 | 27.51 | 11.47x (+1047.4%) |
| CPU Array map lookup (minimal) | minimal | 367.75 | 27.51 | 13.37x (+1236.8%) |
| CUDA Counter (minimal) | minimal | 3536.80 | 35.40 | 99.91x (+9891.0%) |
| Kernel Retsnoop (minimal) | minimal | 49.99 | 35.40 | 1.41x (+41.2%) |
| Thread Histogram (minimal) | minimal | 45.99 | 35.40 | 1.30x (+29.9%) |
| Memory Trace (minimal) | minimal | 55.02 | 35.40 | 1.55x (+55.4%) |



# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:05:51.204233  

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
| Baseline (tiny) | tiny | 27.46 | - | - |
| Baseline (small) | small | 27.36 | - | - |
| Baseline (large) | large | 30.32 | - | - |
| Baseline (xlarge) | xlarge | 161.09 | - | - |
| Baseline (minimal) | minimal | 42.17 | - | - |
| Empty probe (tiny) | tiny | 35.59 | 27.46 | 1.30x (+29.6%) |
| Empty probe (xlarge) | xlarge | 172.77 | 161.09 | 1.07x (+7.3%) |
| Global timer (xlarge) | xlarge | 173.82 | 161.09 | 1.08x (+7.9%) |
| GPU Ringbuf (small) | small | 42.07 | 27.36 | 1.54x (+53.8%) |
| GPU Ringbuf (xlarge) | xlarge | 214.38 | 161.09 | 1.33x (+33.1%) |
| Per-GPU-thread array (small) | small | 35.08 | 27.36 | 1.28x (+28.2%) |
| Memtrace (small) | small | 35.01 | 27.36 | 1.28x (+28.0%) |
| Memtrace (large) | large | 173.20 | 30.32 | 5.71x (+471.2%) |
| GPU Array map update (small) | small | 38.98 | 27.36 | 1.42x (+42.5%) |
| GPU Array map update (large) | large | 244.92 | 30.32 | 8.08x (+707.8%) |
| GPU Array map lookup (small) | small | 33.45 | 27.36 | 1.22x (+22.3%) |
| GPU Array map lookup (large) | large | 69.99 | 30.32 | 2.31x (+130.8%) |
| CPU Array map update (minimal) | minimal | 323.14 | 27.46 | 11.77x (+1076.8%) |
| CPU Array map lookup (minimal) | minimal | 367.08 | 27.46 | 13.37x (+1236.8%) |
| CUDA Counter (minimal) | minimal | 3481.82 | 42.17 | 82.57x (+8156.6%) |
| Kernel Retsnoop (minimal) | minimal | 46.18 | 42.17 | 1.10x (+9.5%) |
| Thread Histogram (minimal) | minimal | 51.65 | 42.17 | 1.22x (+22.5%) |
| Memory Trace (minimal) | minimal | 61.83 | 42.17 | 1.47x (+46.6%) |


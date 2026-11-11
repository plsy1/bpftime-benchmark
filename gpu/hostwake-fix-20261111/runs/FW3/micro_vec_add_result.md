
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:15:29.009167  

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
| Baseline (tiny) | tiny | 27.57 | - | - |
| Baseline (small) | small | 28.01 | - | - |
| Baseline (large) | large | 31.29 | - | - |
| Baseline (xlarge) | xlarge | 159.94 | - | - |
| Baseline (minimal) | minimal | 37.70 | - | - |
| Empty probe (tiny) | tiny | 30.63 | 27.57 | 1.11x (+11.1%) |
| Empty probe (xlarge) | xlarge | 175.37 | 159.94 | 1.10x (+9.6%) |
| Global timer (xlarge) | xlarge | 181.32 | 159.94 | 1.13x (+13.4%) |
| GPU Ringbuf (small) | small | 42.87 | 28.01 | 1.53x (+53.1%) |
| GPU Ringbuf (xlarge) | xlarge | 214.76 | 159.94 | 1.34x (+34.3%) |
| Per-GPU-thread array (small) | small | 37.77 | 28.01 | 1.35x (+34.8%) |
| Memtrace (small) | small | 37.56 | 28.01 | 1.34x (+34.1%) |
| Memtrace (large) | large | 176.35 | 31.29 | 5.64x (+463.6%) |
| GPU Array map update (small) | small | 40.00 | 28.01 | 1.43x (+42.8%) |
| GPU Array map update (large) | large | 239.85 | 31.29 | 7.67x (+666.5%) |
| GPU Array map lookup (small) | small | 32.58 | 28.01 | 1.16x (+16.3%) |
| GPU Array map lookup (large) | large | 69.20 | 31.29 | 2.21x (+121.2%) |
| CPU Array map update (minimal) | minimal | 41.83 | 27.57 | 1.52x (+51.7%) |
| CPU Array map lookup (minimal) | minimal | 41.21 | 27.57 | 1.49x (+49.5%) |
| CUDA Counter (minimal) | minimal | 2935.29 | 37.70 | 77.86x (+7685.9%) |
| Kernel Retsnoop (minimal) | minimal | 46.46 | 37.70 | 1.23x (+23.2%) |
| Thread Histogram (minimal) | minimal | 40.10 | 37.70 | 1.06x (+6.4%) |
| Memory Trace (minimal) | minimal | 63.52 | 37.70 | 1.68x (+68.5%) |


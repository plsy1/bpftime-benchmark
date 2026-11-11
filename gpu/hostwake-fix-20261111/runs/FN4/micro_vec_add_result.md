
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:16:15.783646  

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
| Baseline (tiny) | tiny | 26.73 | - | - |
| Baseline (small) | small | 27.47 | - | - |
| Baseline (large) | large | 31.11 | - | - |
| Baseline (xlarge) | xlarge | 157.94 | - | - |
| Baseline (minimal) | minimal | 45.28 | - | - |
| Empty probe (tiny) | tiny | 36.37 | 26.73 | 1.36x (+36.1%) |
| Empty probe (xlarge) | xlarge | 169.99 | 157.94 | 1.08x (+7.6%) |
| Global timer (xlarge) | xlarge | 179.17 | 157.94 | 1.13x (+13.4%) |
| GPU Ringbuf (small) | small | 41.74 | 27.47 | 1.52x (+51.9%) |
| GPU Ringbuf (xlarge) | xlarge | 214.01 | 157.94 | 1.36x (+35.5%) |
| Per-GPU-thread array (small) | small | 32.02 | 27.47 | 1.17x (+16.6%) |
| Memtrace (small) | small | 37.59 | 27.47 | 1.37x (+36.8%) |
| Memtrace (large) | large | 175.98 | 31.11 | 5.66x (+465.7%) |
| GPU Array map update (small) | small | 39.16 | 27.47 | 1.43x (+42.6%) |
| GPU Array map update (large) | large | 250.28 | 31.11 | 8.05x (+704.5%) |
| GPU Array map lookup (small) | small | 36.07 | 27.47 | 1.31x (+31.3%) |
| GPU Array map lookup (large) | large | 70.20 | 31.11 | 2.26x (+125.7%) |
| CPU Array map update (minimal) | minimal | 49.16 | 26.73 | 1.84x (+83.9%) |
| CPU Array map lookup (minimal) | minimal | 46.94 | 26.73 | 1.76x (+75.6%) |
| CUDA Counter (minimal) | minimal | 2938.16 | 45.28 | 64.89x (+6388.9%) |
| Kernel Retsnoop (minimal) | minimal | 49.45 | 45.28 | 1.09x (+9.2%) |
| Thread Histogram (minimal) | minimal | 50.70 | 45.28 | 1.12x (+12.0%) |
| Memory Trace (minimal) | minimal | 57.44 | 45.28 | 1.27x (+26.9%) |


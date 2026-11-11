
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:15:46.251267  

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
| Baseline (tiny) | tiny | 26.96 | - | - |
| Baseline (small) | small | 26.96 | - | - |
| Baseline (large) | large | 31.15 | - | - |
| Baseline (xlarge) | xlarge | 159.86 | - | - |
| Baseline (minimal) | minimal | 52.36 | - | - |
| Empty probe (tiny) | tiny | 32.74 | 26.96 | 1.21x (+21.4%) |
| Empty probe (xlarge) | xlarge | 174.78 | 159.86 | 1.09x (+9.3%) |
| Global timer (xlarge) | xlarge | 176.56 | 159.86 | 1.10x (+10.4%) |
| GPU Ringbuf (small) | small | 40.70 | 26.96 | 1.51x (+51.0%) |
| GPU Ringbuf (xlarge) | xlarge | 215.75 | 159.86 | 1.35x (+35.0%) |
| Per-GPU-thread array (small) | small | 34.54 | 26.96 | 1.28x (+28.1%) |
| Memtrace (small) | small | 37.39 | 26.96 | 1.39x (+38.7%) |
| Memtrace (large) | large | 175.43 | 31.15 | 5.63x (+463.2%) |
| GPU Array map update (small) | small | 38.25 | 26.96 | 1.42x (+41.9%) |
| GPU Array map update (large) | large | 238.39 | 31.15 | 7.65x (+665.3%) |
| GPU Array map lookup (small) | small | 38.97 | 26.96 | 1.45x (+44.5%) |
| GPU Array map lookup (large) | large | 66.31 | 31.15 | 2.13x (+112.9%) |
| CPU Array map update (minimal) | minimal | 325.26 | 26.96 | 12.06x (+1106.5%) |
| CPU Array map lookup (minimal) | minimal | 375.67 | 26.96 | 13.93x (+1293.4%) |
| CUDA Counter (minimal) | minimal | 3480.64 | 52.36 | 66.48x (+6547.5%) |
| Kernel Retsnoop (minimal) | minimal | 49.91 | 52.36 | 0.95x (-4.7%) |
| Thread Histogram (minimal) | minimal | 47.67 | 52.36 | 0.91x (-9.0%) |
| Memory Trace (minimal) | minimal | 58.14 | 52.36 | 1.11x (+11.0%) |


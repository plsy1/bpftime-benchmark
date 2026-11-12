
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:25:18.078994  

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
| Baseline (tiny) | tiny | 26.86 | - | - |
| Baseline (small) | small | 27.33 | - | - |
| Baseline (large) | large | 31.21 | - | - |
| Baseline (xlarge) | xlarge | 157.70 | - | - |
| Baseline (minimal) | minimal | 41.59 | - | - |
| Empty probe (tiny) | tiny | 30.80 | 26.86 | 1.15x (+14.7%) |
| Empty probe (xlarge) | xlarge | 175.44 | 157.70 | 1.11x (+11.2%) |
| Global timer (xlarge) | xlarge | 182.71 | 157.70 | 1.16x (+15.9%) |
| GPU Ringbuf (small) | small | 44.19 | 27.33 | 1.62x (+61.7%) |
| GPU Ringbuf (xlarge) | xlarge | 215.54 | 157.70 | 1.37x (+36.7%) |
| Per-GPU-thread array (small) | small | 36.63 | 27.33 | 1.34x (+34.0%) |
| Memtrace (small) | small | 35.63 | 27.33 | 1.30x (+30.4%) |
| Memtrace (large) | large | 175.78 | 31.21 | 5.63x (+463.2%) |
| GPU Array map update (small) | small | 41.30 | 27.33 | 1.51x (+51.1%) |
| GPU Array map update (large) | large | 224.45 | 31.21 | 7.19x (+619.2%) |
| GPU Array map lookup (small) | small | 35.13 | 27.33 | 1.29x (+28.5%) |
| GPU Array map lookup (large) | large | 70.61 | 31.21 | 2.26x (+126.2%) |
| CPU Array map update (minimal) | minimal | 52.13 | 26.86 | 1.94x (+94.1%) |
| CPU Array map lookup (minimal) | minimal | 39.64 | 26.86 | 1.48x (+47.6%) |
| CUDA Counter (minimal) | minimal | 3017.11 | 41.59 | 72.54x (+7154.4%) |
| Kernel Retsnoop (minimal) | minimal | 47.53 | 41.59 | 1.14x (+14.3%) |
| Thread Histogram (minimal) | minimal | 54.75 | 41.59 | 1.32x (+31.6%) |
| Memory Trace (minimal) | minimal | 61.27 | 41.59 | 1.47x (+47.3%) |


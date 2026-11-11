
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:06:39.027781  

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
| Baseline (tiny) | tiny | 28.34 | - | - |
| Baseline (small) | small | 27.15 | - | - |
| Baseline (large) | large | 31.37 | - | - |
| Baseline (xlarge) | xlarge | 158.79 | - | - |
| Baseline (minimal) | minimal | 37.75 | - | - |
| Empty probe (tiny) | tiny | 30.20 | 28.34 | 1.07x (+6.6%) |
| Empty probe (xlarge) | xlarge | 171.65 | 158.79 | 1.08x (+8.1%) |
| Global timer (xlarge) | xlarge | 175.77 | 158.79 | 1.11x (+10.7%) |
| GPU Ringbuf (small) | small | 42.80 | 27.15 | 1.58x (+57.6%) |
| GPU Ringbuf (xlarge) | xlarge | 212.39 | 158.79 | 1.34x (+33.8%) |
| Per-GPU-thread array (small) | small | 39.63 | 27.15 | 1.46x (+46.0%) |
| Memtrace (small) | small | 32.03 | 27.15 | 1.18x (+18.0%) |
| Memtrace (large) | large | 173.22 | 31.37 | 5.52x (+452.2%) |
| GPU Array map update (small) | small | 32.61 | 27.15 | 1.20x (+20.1%) |
| GPU Array map update (large) | large | 242.70 | 31.37 | 7.74x (+673.7%) |
| GPU Array map lookup (small) | small | 31.39 | 27.15 | 1.16x (+15.6%) |
| GPU Array map lookup (large) | large | 69.13 | 31.37 | 2.20x (+120.4%) |
| CPU Array map update (minimal) | minimal | 45.08 | 28.34 | 1.59x (+59.1%) |
| CPU Array map lookup (minimal) | minimal | 45.79 | 28.34 | 1.62x (+61.6%) |
| CUDA Counter (minimal) | minimal | 2887.61 | 37.75 | 76.49x (+7549.3%) |
| Kernel Retsnoop (minimal) | minimal | 53.32 | 37.75 | 1.41x (+41.2%) |
| Thread Histogram (minimal) | minimal | 51.30 | 37.75 | 1.36x (+35.9%) |
| Memory Trace (minimal) | minimal | 57.36 | 37.75 | 1.52x (+51.9%) |


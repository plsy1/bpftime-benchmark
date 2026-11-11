
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:44:06.503473  

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
| Baseline (tiny) | tiny | 27.43 | - | - |
| Baseline (small) | small | 27.72 | - | - |
| Baseline (large) | large | 31.45 | - | - |
| Baseline (xlarge) | xlarge | 160.88 | - | - |
| Baseline (minimal) | minimal | 39.08 | - | - |
| Empty probe (tiny) | tiny | 30.62 | 27.43 | 1.12x (+11.6%) |
| Empty probe (xlarge) | xlarge | 172.88 | 160.88 | 1.07x (+7.5%) |
| Global timer (xlarge) | xlarge | 179.64 | 160.88 | 1.12x (+11.7%) |
| GPU Ringbuf (small) | small | 41.41 | 27.72 | 1.49x (+49.4%) |
| GPU Ringbuf (xlarge) | xlarge | 216.78 | 160.88 | 1.35x (+34.7%) |
| Per-GPU-thread array (small) | small | 37.35 | 27.72 | 1.35x (+34.7%) |
| Memtrace (small) | small | 34.39 | 27.72 | 1.24x (+24.1%) |
| Memtrace (large) | large | 174.33 | 31.45 | 5.54x (+454.3%) |
| GPU Array map update (small) | small | 37.12 | 27.72 | 1.34x (+33.9%) |
| GPU Array map update (large) | large | 226.45 | 31.45 | 7.20x (+620.0%) |
| GPU Array map lookup (small) | small | 33.51 | 27.72 | 1.21x (+20.9%) |
| GPU Array map lookup (large) | large | 70.61 | 31.45 | 2.25x (+124.5%) |
| CPU Array map update (minimal) | minimal | 48.83 | 27.43 | 1.78x (+78.0%) |
| CPU Array map lookup (minimal) | minimal | 54.38 | 27.43 | 1.98x (+98.3%) |
| CUDA Counter (minimal) | minimal | 2919.94 | 39.08 | 74.72x (+7371.7%) |
| Kernel Retsnoop (minimal) | minimal | 48.65 | 39.08 | 1.24x (+24.5%) |
| Thread Histogram (minimal) | minimal | 52.05 | 39.08 | 1.33x (+33.2%) |
| Memory Trace (minimal) | minimal | 59.07 | 39.08 | 1.51x (+51.2%) |


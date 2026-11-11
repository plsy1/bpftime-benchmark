
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:51:13.942254  

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
| Baseline (tiny) | tiny | 28.10 | - | - |
| Baseline (small) | small | 27.67 | - | - |
| Baseline (large) | large | 30.48 | - | - |
| Baseline (xlarge) | xlarge | 159.49 | - | - |
| Baseline (minimal) | minimal | 43.30 | - | - |
| Empty probe (tiny) | tiny | 32.84 | 28.10 | 1.17x (+16.9%) |
| Empty probe (xlarge) | xlarge | 177.29 | 159.49 | 1.11x (+11.2%) |
| Global timer (xlarge) | xlarge | 174.63 | 159.49 | 1.09x (+9.5%) |
| GPU Ringbuf (small) | small | 41.88 | 27.67 | 1.51x (+51.4%) |
| GPU Ringbuf (xlarge) | xlarge | 215.10 | 159.49 | 1.35x (+34.9%) |
| Per-GPU-thread array (small) | small | 33.52 | 27.67 | 1.21x (+21.1%) |
| Memtrace (small) | small | 32.61 | 27.67 | 1.18x (+17.9%) |
| Memtrace (large) | large | 170.56 | 30.48 | 5.60x (+459.6%) |
| GPU Array map update (small) | small | 38.59 | 27.67 | 1.39x (+39.5%) |
| GPU Array map update (large) | large | 247.40 | 30.48 | 8.12x (+711.7%) |
| GPU Array map lookup (small) | small | 41.63 | 27.67 | 1.50x (+50.5%) |
| GPU Array map lookup (large) | large | 65.65 | 30.48 | 2.15x (+115.4%) |
| CPU Array map update (minimal) | minimal | 54.51 | 28.10 | 1.94x (+94.0%) |
| CPU Array map lookup (minimal) | minimal | 48.72 | 28.10 | 1.73x (+73.4%) |
| CUDA Counter (minimal) | minimal | 2885.29 | 43.30 | 66.63x (+6563.5%) |
| Kernel Retsnoop (minimal) | minimal | 49.76 | 43.30 | 1.15x (+14.9%) |
| Thread Histogram (minimal) | minimal | 59.56 | 43.30 | 1.38x (+37.6%) |
| Memory Trace (minimal) | minimal | 61.35 | 43.30 | 1.42x (+41.7%) |



# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:22:32.132430  

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
| Baseline (tiny) | tiny | 26.85 | - | - |
| Baseline (small) | small | 27.54 | - | - |
| Baseline (large) | large | 32.23 | - | - |
| Baseline (xlarge) | xlarge | 160.61 | - | - |
| Baseline (minimal) | minimal | 39.25 | - | - |
| Empty probe (tiny) | tiny | 27.33 | 26.85 | 1.02x (+1.8%) |
| Empty probe (xlarge) | xlarge | 177.46 | 160.61 | 1.10x (+10.5%) |
| Global timer (xlarge) | xlarge | 176.81 | 160.61 | 1.10x (+10.1%) |
| GPU Ringbuf (small) | small | 47.49 | 27.54 | 1.72x (+72.4%) |
| GPU Ringbuf (xlarge) | xlarge | 219.27 | 160.61 | 1.37x (+36.5%) |
| Per-GPU-thread array (small) | small | 37.06 | 27.54 | 1.35x (+34.6%) |
| Memtrace (small) | small | 36.49 | 27.54 | 1.32x (+32.5%) |
| Memtrace (large) | large | 173.70 | 32.23 | 5.39x (+438.9%) |
| GPU Array map update (small) | small | 39.64 | 27.54 | 1.44x (+43.9%) |
| GPU Array map update (large) | large | 247.67 | 32.23 | 7.68x (+668.4%) |
| GPU Array map lookup (small) | small | 35.31 | 27.54 | 1.28x (+28.2%) |
| GPU Array map lookup (large) | large | 73.31 | 32.23 | 2.27x (+127.5%) |
| CPU Array map update (minimal) | minimal | 46.99 | 26.85 | 1.75x (+75.0%) |
| CPU Array map lookup (minimal) | minimal | 46.25 | 26.85 | 1.72x (+72.3%) |
| CUDA Counter (minimal) | minimal | 2921.27 | 39.25 | 74.43x (+7342.7%) |
| Kernel Retsnoop (minimal) | minimal | 45.97 | 39.25 | 1.17x (+17.1%) |
| Thread Histogram (minimal) | minimal | 45.83 | 39.25 | 1.17x (+16.8%) |
| Memory Trace (minimal) | minimal | 57.10 | 39.25 | 1.45x (+45.5%) |


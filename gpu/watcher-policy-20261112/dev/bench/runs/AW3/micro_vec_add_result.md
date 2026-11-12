
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:22:55.950080  

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
| Baseline (tiny) | tiny | 27.77 | - | - |
| Baseline (small) | small | 28.46 | - | - |
| Baseline (large) | large | 31.40 | - | - |
| Baseline (xlarge) | xlarge | 158.79 | - | - |
| Baseline (minimal) | minimal | 45.83 | - | - |
| Empty probe (tiny) | tiny | 28.26 | 27.77 | 1.02x (+1.8%) |
| Empty probe (xlarge) | xlarge | 172.85 | 158.79 | 1.09x (+8.9%) |
| Global timer (xlarge) | xlarge | 179.07 | 158.79 | 1.13x (+12.8%) |
| GPU Ringbuf (small) | small | 47.45 | 28.46 | 1.67x (+66.7%) |
| GPU Ringbuf (xlarge) | xlarge | 216.64 | 158.79 | 1.36x (+36.4%) |
| Per-GPU-thread array (small) | small | 36.27 | 28.46 | 1.27x (+27.4%) |
| Memtrace (small) | small | 35.32 | 28.46 | 1.24x (+24.1%) |
| Memtrace (large) | large | 175.50 | 31.40 | 5.59x (+458.9%) |
| GPU Array map update (small) | small | 37.19 | 28.46 | 1.31x (+30.7%) |
| GPU Array map update (large) | large | 231.62 | 31.40 | 7.38x (+637.6%) |
| GPU Array map lookup (small) | small | 35.64 | 28.46 | 1.25x (+25.2%) |
| GPU Array map lookup (large) | large | 68.87 | 31.40 | 2.19x (+119.3%) |
| CPU Array map update (minimal) | minimal | 38.77 | 27.77 | 1.40x (+39.6%) |
| CPU Array map lookup (minimal) | minimal | 48.35 | 27.77 | 1.74x (+74.1%) |
| CUDA Counter (minimal) | minimal | 2907.02 | 45.83 | 63.43x (+6243.1%) |
| Kernel Retsnoop (minimal) | minimal | 48.27 | 45.83 | 1.05x (+5.3%) |
| Thread Histogram (minimal) | minimal | 40.05 | 45.83 | 0.87x (-12.6%) |
| Memory Trace (minimal) | minimal | 60.36 | 45.83 | 1.32x (+31.7%) |


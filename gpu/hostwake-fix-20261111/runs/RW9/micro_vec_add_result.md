
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:50:26.287494  

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
| Baseline (tiny) | tiny | 27.36 | - | - |
| Baseline (small) | small | 26.35 | - | - |
| Baseline (large) | large | 31.21 | - | - |
| Baseline (xlarge) | xlarge | 159.89 | - | - |
| Baseline (minimal) | minimal | 40.60 | - | - |
| Empty probe (tiny) | tiny | 29.53 | 27.36 | 1.08x (+7.9%) |
| Empty probe (xlarge) | xlarge | 178.62 | 159.89 | 1.12x (+11.7%) |
| Global timer (xlarge) | xlarge | 184.48 | 159.89 | 1.15x (+15.4%) |
| GPU Ringbuf (small) | small | 47.22 | 26.35 | 1.79x (+79.2%) |
| GPU Ringbuf (xlarge) | xlarge | 218.92 | 159.89 | 1.37x (+36.9%) |
| Per-GPU-thread array (small) | small | 37.87 | 26.35 | 1.44x (+43.7%) |
| Memtrace (small) | small | 35.20 | 26.35 | 1.34x (+33.6%) |
| Memtrace (large) | large | 173.96 | 31.21 | 5.57x (+457.4%) |
| GPU Array map update (small) | small | 37.81 | 26.35 | 1.43x (+43.5%) |
| GPU Array map update (large) | large | 227.94 | 31.21 | 7.30x (+630.3%) |
| GPU Array map lookup (small) | small | 33.30 | 26.35 | 1.26x (+26.4%) |
| GPU Array map lookup (large) | large | 69.55 | 31.21 | 2.23x (+122.8%) |
| CPU Array map update (minimal) | minimal | 42.23 | 27.36 | 1.54x (+54.3%) |
| CPU Array map lookup (minimal) | minimal | 41.29 | 27.36 | 1.51x (+50.9%) |
| CUDA Counter (minimal) | minimal | 3071.05 | 40.60 | 75.64x (+7464.2%) |
| Kernel Retsnoop (minimal) | minimal | 46.90 | 40.60 | 1.16x (+15.5%) |
| Thread Histogram (minimal) | minimal | 51.03 | 40.60 | 1.26x (+25.7%) |
| Memory Trace (minimal) | minimal | 61.76 | 40.60 | 1.52x (+52.1%) |


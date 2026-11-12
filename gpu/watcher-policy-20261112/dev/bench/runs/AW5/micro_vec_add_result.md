
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:24:30.301069  

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
| Baseline (tiny) | tiny | 26.08 | - | - |
| Baseline (small) | small | 27.51 | - | - |
| Baseline (large) | large | 31.47 | - | - |
| Baseline (xlarge) | xlarge | 159.57 | - | - |
| Baseline (minimal) | minimal | 37.63 | - | - |
| Empty probe (tiny) | tiny | 31.42 | 26.08 | 1.20x (+20.5%) |
| Empty probe (xlarge) | xlarge | 178.17 | 159.57 | 1.12x (+11.7%) |
| Global timer (xlarge) | xlarge | 177.32 | 159.57 | 1.11x (+11.1%) |
| GPU Ringbuf (small) | small | 44.22 | 27.51 | 1.61x (+60.7%) |
| GPU Ringbuf (xlarge) | xlarge | 214.76 | 159.57 | 1.35x (+34.6%) |
| Per-GPU-thread array (small) | small | 35.71 | 27.51 | 1.30x (+29.8%) |
| Memtrace (small) | small | 37.53 | 27.51 | 1.36x (+36.4%) |
| Memtrace (large) | large | 174.64 | 31.47 | 5.55x (+454.9%) |
| GPU Array map update (small) | small | 36.81 | 27.51 | 1.34x (+33.8%) |
| GPU Array map update (large) | large | 234.43 | 31.47 | 7.45x (+644.9%) |
| GPU Array map lookup (small) | small | 35.03 | 27.51 | 1.27x (+27.3%) |
| GPU Array map lookup (large) | large | 68.12 | 31.47 | 2.16x (+116.5%) |
| CPU Array map update (minimal) | minimal | 39.00 | 26.08 | 1.50x (+49.5%) |
| CPU Array map lookup (minimal) | minimal | 47.43 | 26.08 | 1.82x (+81.9%) |
| CUDA Counter (minimal) | minimal | 2911.49 | 37.63 | 77.37x (+7637.2%) |
| Kernel Retsnoop (minimal) | minimal | 50.28 | 37.63 | 1.34x (+33.6%) |
| Thread Histogram (minimal) | minimal | 40.72 | 37.63 | 1.08x (+8.2%) |
| Memory Trace (minimal) | minimal | 63.91 | 37.63 | 1.70x (+69.8%) |


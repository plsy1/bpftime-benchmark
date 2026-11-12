
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:26:05.909418  

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
| Baseline (tiny) | tiny | 28.04 | - | - |
| Baseline (small) | small | 27.08 | - | - |
| Baseline (large) | large | 31.14 | - | - |
| Baseline (xlarge) | xlarge | 159.34 | - | - |
| Baseline (minimal) | minimal | 40.49 | - | - |
| Empty probe (tiny) | tiny | 28.63 | 28.04 | 1.02x (+2.1%) |
| Empty probe (xlarge) | xlarge | 174.16 | 159.34 | 1.09x (+9.3%) |
| Global timer (xlarge) | xlarge | 175.76 | 159.34 | 1.10x (+10.3%) |
| GPU Ringbuf (small) | small | 41.85 | 27.08 | 1.55x (+54.5%) |
| GPU Ringbuf (xlarge) | xlarge | 213.97 | 159.34 | 1.34x (+34.3%) |
| Per-GPU-thread array (small) | small | 37.34 | 27.08 | 1.38x (+37.9%) |
| Memtrace (small) | small | 35.23 | 27.08 | 1.30x (+30.1%) |
| Memtrace (large) | large | 179.72 | 31.14 | 5.77x (+477.1%) |
| GPU Array map update (small) | small | 35.77 | 27.08 | 1.32x (+32.1%) |
| GPU Array map update (large) | large | 241.71 | 31.14 | 7.76x (+676.2%) |
| GPU Array map lookup (small) | small | 30.36 | 27.08 | 1.12x (+12.1%) |
| GPU Array map lookup (large) | large | 65.07 | 31.14 | 2.09x (+109.0%) |
| CPU Array map update (minimal) | minimal | 44.10 | 28.04 | 1.57x (+57.3%) |
| CPU Array map lookup (minimal) | minimal | 41.18 | 28.04 | 1.47x (+46.9%) |
| CUDA Counter (minimal) | minimal | 2919.95 | 40.49 | 72.12x (+7111.5%) |
| Kernel Retsnoop (minimal) | minimal | 50.13 | 40.49 | 1.24x (+23.8%) |
| Thread Histogram (minimal) | minimal | 49.42 | 40.49 | 1.22x (+22.1%) |
| Memory Trace (minimal) | minimal | 61.90 | 40.49 | 1.53x (+52.9%) |


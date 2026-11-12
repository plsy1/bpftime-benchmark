
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:20:50.505680  

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
| Baseline (tiny) | tiny | 25.56 | - | - |
| Baseline (small) | small | 27.55 | - | - |
| Baseline (large) | large | 31.21 | - | - |
| Baseline (xlarge) | xlarge | 159.16 | - | - |
| Baseline (minimal) | minimal | 40.18 | - | - |
| Empty probe (tiny) | tiny | 29.46 | 25.56 | 1.15x (+15.3%) |
| Empty probe (xlarge) | xlarge | 177.24 | 159.16 | 1.11x (+11.4%) |
| Global timer (xlarge) | xlarge | 173.63 | 159.16 | 1.09x (+9.1%) |
| GPU Ringbuf (small) | small | 45.32 | 27.55 | 1.65x (+64.5%) |
| GPU Ringbuf (xlarge) | xlarge | 217.22 | 159.16 | 1.36x (+36.5%) |
| Per-GPU-thread array (small) | small | 38.07 | 27.55 | 1.38x (+38.2%) |
| Memtrace (small) | small | 35.62 | 27.55 | 1.29x (+29.3%) |
| Memtrace (large) | large | 173.17 | 31.21 | 5.55x (+454.9%) |
| GPU Array map update (small) | small | 38.45 | 27.55 | 1.40x (+39.6%) |
| GPU Array map update (large) | large | 237.59 | 31.21 | 7.61x (+661.3%) |
| GPU Array map lookup (small) | small | 34.67 | 27.55 | 1.26x (+25.8%) |
| GPU Array map lookup (large) | large | 67.74 | 31.21 | 2.17x (+117.0%) |
| CPU Array map update (minimal) | minimal | 39.60 | 25.56 | 1.55x (+54.9%) |
| CPU Array map lookup (minimal) | minimal | 51.93 | 25.56 | 2.03x (+103.2%) |
| CUDA Counter (minimal) | minimal | 2948.10 | 40.18 | 73.37x (+7237.2%) |
| Kernel Retsnoop (minimal) | minimal | 46.67 | 40.18 | 1.16x (+16.2%) |
| Thread Histogram (minimal) | minimal | 44.30 | 40.18 | 1.10x (+10.3%) |
| Memory Trace (minimal) | minimal | 63.49 | 40.18 | 1.58x (+58.0%) |


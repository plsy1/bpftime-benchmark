
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:25:41.996597  

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
| Baseline (tiny) | tiny | 27.48 | - | - |
| Baseline (small) | small | 27.16 | - | - |
| Baseline (large) | large | 31.22 | - | - |
| Baseline (xlarge) | xlarge | 159.57 | - | - |
| Baseline (minimal) | minimal | 38.87 | - | - |
| Empty probe (tiny) | tiny | 29.00 | 27.48 | 1.06x (+5.5%) |
| Empty probe (xlarge) | xlarge | 177.07 | 159.57 | 1.11x (+11.0%) |
| Global timer (xlarge) | xlarge | 179.39 | 159.57 | 1.12x (+12.4%) |
| GPU Ringbuf (small) | small | 43.55 | 27.16 | 1.60x (+60.3%) |
| GPU Ringbuf (xlarge) | xlarge | 216.35 | 159.57 | 1.36x (+35.6%) |
| Per-GPU-thread array (small) | small | 37.87 | 27.16 | 1.39x (+39.4%) |
| Memtrace (small) | small | 36.30 | 27.16 | 1.34x (+33.7%) |
| Memtrace (large) | large | 172.24 | 31.22 | 5.52x (+451.7%) |
| GPU Array map update (small) | small | 37.14 | 27.16 | 1.37x (+36.7%) |
| GPU Array map update (large) | large | 229.28 | 31.22 | 7.34x (+634.4%) |
| GPU Array map lookup (small) | small | 30.50 | 27.16 | 1.12x (+12.3%) |
| GPU Array map lookup (large) | large | 71.41 | 31.22 | 2.29x (+128.7%) |
| CPU Array map update (minimal) | minimal | 47.02 | 27.48 | 1.71x (+71.1%) |
| CPU Array map lookup (minimal) | minimal | 47.02 | 27.48 | 1.71x (+71.1%) |
| CUDA Counter (minimal) | minimal | 2959.24 | 38.87 | 76.13x (+7513.2%) |
| Kernel Retsnoop (minimal) | minimal | 48.54 | 38.87 | 1.25x (+24.9%) |
| Thread Histogram (minimal) | minimal | 49.38 | 38.87 | 1.27x (+27.0%) |
| Memory Trace (minimal) | minimal | 56.30 | 38.87 | 1.45x (+44.8%) |


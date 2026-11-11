
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:44:30.532582  

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
| Baseline (tiny) | tiny | 27.60 | - | - |
| Baseline (small) | small | 26.77 | - | - |
| Baseline (large) | large | 31.54 | - | - |
| Baseline (xlarge) | xlarge | 160.71 | - | - |
| Baseline (minimal) | minimal | 38.11 | - | - |
| Empty probe (tiny) | tiny | 29.72 | 27.60 | 1.08x (+7.7%) |
| Empty probe (xlarge) | xlarge | 175.28 | 160.71 | 1.09x (+9.1%) |
| Global timer (xlarge) | xlarge | 180.86 | 160.71 | 1.13x (+12.5%) |
| GPU Ringbuf (small) | small | 49.47 | 26.77 | 1.85x (+84.8%) |
| GPU Ringbuf (xlarge) | xlarge | 213.97 | 160.71 | 1.33x (+33.1%) |
| Per-GPU-thread array (small) | small | 36.49 | 26.77 | 1.36x (+36.3%) |
| Memtrace (small) | small | 37.14 | 26.77 | 1.39x (+38.7%) |
| Memtrace (large) | large | 173.54 | 31.54 | 5.50x (+450.2%) |
| GPU Array map update (small) | small | 40.15 | 26.77 | 1.50x (+50.0%) |
| GPU Array map update (large) | large | 237.88 | 31.54 | 7.54x (+654.2%) |
| GPU Array map lookup (small) | small | 35.65 | 26.77 | 1.33x (+33.2%) |
| GPU Array map lookup (large) | large | 72.04 | 31.54 | 2.28x (+128.4%) |
| CPU Array map update (minimal) | minimal | 39.32 | 27.60 | 1.42x (+42.5%) |
| CPU Array map lookup (minimal) | minimal | 46.12 | 27.60 | 1.67x (+67.1%) |
| CUDA Counter (minimal) | minimal | 2894.04 | 38.11 | 75.94x (+7493.9%) |
| Kernel Retsnoop (minimal) | minimal | 47.98 | 38.11 | 1.26x (+25.9%) |
| Thread Histogram (minimal) | minimal | 50.72 | 38.11 | 1.33x (+33.1%) |
| Memory Trace (minimal) | minimal | 59.13 | 38.11 | 1.55x (+55.2%) |


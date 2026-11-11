
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:27:48.265194  

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
| Baseline (tiny) | tiny | 27.16 | - | - |
| Baseline (small) | small | 27.30 | - | - |
| Baseline (large) | large | 31.22 | - | - |
| Baseline (xlarge) | xlarge | 158.78 | - | - |
| Baseline (minimal) | minimal | 39.84 | - | - |
| Empty probe (tiny) | tiny | 33.66 | 27.16 | 1.24x (+23.9%) |
| Empty probe (xlarge) | xlarge | 180.09 | 158.78 | 1.13x (+13.4%) |
| Global timer (xlarge) | xlarge | 180.58 | 158.78 | 1.14x (+13.7%) |
| GPU Ringbuf (small) | small | 40.34 | 27.30 | 1.48x (+47.8%) |
| GPU Ringbuf (xlarge) | xlarge | 207.62 | 158.78 | 1.31x (+30.8%) |
| Per-GPU-thread array (small) | small | 37.99 | 27.30 | 1.39x (+39.2%) |
| Memtrace (small) | small | 38.01 | 27.30 | 1.39x (+39.2%) |
| Memtrace (large) | large | 184.85 | 31.22 | 5.92x (+492.1%) |
| GPU Array map update (small) | small | 40.58 | 27.30 | 1.49x (+48.6%) |
| GPU Array map update (large) | large | 222.59 | 31.22 | 7.13x (+613.0%) |
| GPU Array map lookup (small) | small | 38.04 | 27.30 | 1.39x (+39.3%) |
| GPU Array map lookup (large) | large | 70.54 | 31.22 | 2.26x (+125.9%) |
| CPU Array map update (minimal) | minimal | 33686.38 | 27.16 | 1240.29x (+123929.4%) |
| CPU Array map lookup (minimal) | minimal | 33695.07 | 27.16 | 1240.61x (+123961.4%) |
| CUDA Counter (minimal) | minimal | 337316.54 | 39.84 | 8466.78x (+846578.1%) |
| Kernel Retsnoop (minimal) | minimal | 60.60 | 39.84 | 1.52x (+52.1%) |
| Thread Histogram (minimal) | minimal | 52.65 | 39.84 | 1.32x (+32.2%) |
| Memory Trace (minimal) | minimal | 69.78 | 39.84 | 1.75x (+75.2%) |


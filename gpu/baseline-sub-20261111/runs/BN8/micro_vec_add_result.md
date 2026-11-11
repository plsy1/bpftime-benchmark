
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:24:18.748693  

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
| Baseline (tiny) | tiny | 27.33 | - | - |
| Baseline (small) | small | 26.38 | - | - |
| Baseline (large) | large | 31.06 | - | - |
| Baseline (xlarge) | xlarge | 160.26 | - | - |
| Baseline (minimal) | minimal | 35.63 | - | - |
| Empty probe (tiny) | tiny | 35.80 | 27.33 | 1.31x (+31.0%) |
| Empty probe (xlarge) | xlarge | 173.41 | 160.26 | 1.08x (+8.2%) |
| Global timer (xlarge) | xlarge | 176.11 | 160.26 | 1.10x (+9.9%) |
| GPU Ringbuf (small) | small | 42.03 | 26.38 | 1.59x (+59.3%) |
| GPU Ringbuf (xlarge) | xlarge | 216.97 | 160.26 | 1.35x (+35.4%) |
| Per-GPU-thread array (small) | small | 39.05 | 26.38 | 1.48x (+48.0%) |
| Memtrace (small) | small | 38.01 | 26.38 | 1.44x (+44.1%) |
| Memtrace (large) | large | 178.24 | 31.06 | 5.74x (+473.9%) |
| GPU Array map update (small) | small | 36.98 | 26.38 | 1.40x (+40.2%) |
| GPU Array map update (large) | large | 230.67 | 31.06 | 7.43x (+642.7%) |
| GPU Array map lookup (small) | small | 34.88 | 26.38 | 1.32x (+32.2%) |
| GPU Array map lookup (large) | large | 65.88 | 31.06 | 2.12x (+112.1%) |
| CPU Array map update (minimal) | minimal | 50.68 | 27.33 | 1.85x (+85.4%) |
| CPU Array map lookup (minimal) | minimal | 48.83 | 27.33 | 1.79x (+78.7%) |
| CUDA Counter (minimal) | minimal | 2930.40 | 35.63 | 82.25x (+8124.5%) |
| Kernel Retsnoop (minimal) | minimal | 46.65 | 35.63 | 1.31x (+30.9%) |
| Thread Histogram (minimal) | minimal | 49.22 | 35.63 | 1.38x (+38.1%) |
| Memory Trace (minimal) | minimal | 57.34 | 35.63 | 1.61x (+60.9%) |


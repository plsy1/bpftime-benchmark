
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:17:25.720181  

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
| Baseline (tiny) | tiny | 27.12 | - | - |
| Baseline (small) | small | 27.67 | - | - |
| Baseline (large) | large | 31.06 | - | - |
| Baseline (xlarge) | xlarge | 159.74 | - | - |
| Baseline (minimal) | minimal | 38.75 | - | - |
| Empty probe (tiny) | tiny | 28.12 | 27.12 | 1.04x (+3.7%) |
| Empty probe (xlarge) | xlarge | 178.29 | 159.74 | 1.12x (+11.6%) |
| Global timer (xlarge) | xlarge | 176.57 | 159.74 | 1.11x (+10.5%) |
| GPU Ringbuf (small) | small | 49.04 | 27.67 | 1.77x (+77.2%) |
| GPU Ringbuf (xlarge) | xlarge | 215.00 | 159.74 | 1.35x (+34.6%) |
| Per-GPU-thread array (small) | small | 36.91 | 27.67 | 1.33x (+33.4%) |
| Memtrace (small) | small | 36.07 | 27.67 | 1.30x (+30.4%) |
| Memtrace (large) | large | 175.88 | 31.06 | 5.66x (+466.3%) |
| GPU Array map update (small) | small | 38.42 | 27.67 | 1.39x (+38.9%) |
| GPU Array map update (large) | large | 230.55 | 31.06 | 7.42x (+642.3%) |
| GPU Array map lookup (small) | small | 33.31 | 27.67 | 1.20x (+20.4%) |
| GPU Array map lookup (large) | large | 65.35 | 31.06 | 2.10x (+110.4%) |
| CPU Array map update (minimal) | minimal | 40.39 | 27.12 | 1.49x (+48.9%) |
| CPU Array map lookup (minimal) | minimal | 49.13 | 27.12 | 1.81x (+81.2%) |
| CUDA Counter (minimal) | minimal | 2835.87 | 38.75 | 73.18x (+7218.4%) |
| Kernel Retsnoop (minimal) | minimal | 49.34 | 38.75 | 1.27x (+27.3%) |
| Thread Histogram (minimal) | minimal | 38.05 | 38.75 | 0.98x (-1.8%) |
| Memory Trace (minimal) | minimal | 63.37 | 38.75 | 1.64x (+63.5%) |

